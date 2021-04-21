#pragma once

#include <vector>
#include <unordered_map>
#include <map>
#include <memory>
#include <cassert>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <condition_variable>
#include <ichor/interfaces/IFrameworkLogger.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include <ichor/Events.h>
#include <ichor/Callbacks.h>
#include <ichor/Filter.h>
#include <ichor/DependencyRegistrations.h>
#include <ichor/stl/RealtimeMutex.h>
#include <ichor/stl/RealtimeReadWriteMutex.h>
#include <ichor/stl/ConditionVariable.h>
#include <ichor/stl/ConditionVariableAny.h>
#include <ichor/stl/EventStackUniquePtr.h>

// prevent false positives by TSAN
// See "ThreadSanitizer – data race detection in practice" by Serebryany et al. for more info: https://static.googleusercontent.com/media/research.google.com/en//pubs/archive/35604.pdf
#if defined(__SANITIZE_THREAD__)
#define TSAN_ENABLED
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define TSAN_ENABLED
#endif
#endif

#ifdef TSAN_ENABLED
#define TSAN_ANNOTATE_HAPPENS_BEFORE(addr) \
    AnnotateHappensBefore(__FILE__, __LINE__, (void*)(addr))
#define TSAN_ANNOTATE_HAPPENS_AFTER(addr) \
    AnnotateHappensAfter(__FILE__, __LINE__, (void*)(addr))
extern "C" void AnnotateHappensBefore(const char* f, int l, void* addr);
extern "C" void AnnotateHappensAfter(const char* f, int l, void* addr);
#else
#define TSAN_ANNOTATE_HAPPENS_BEFORE(addr)
#define TSAN_ANNOTATE_HAPPENS_AFTER(addr)
#endif

using namespace std::chrono_literals;

namespace Ichor {
    class CommunicationChannel;

    struct DependencyTrackerInfo final {
        explicit DependencyTrackerInfo(Ichor::function<void(Event const * const)> _trackFunc) noexcept : trackFunc(std::move(_trackFunc)) {}
        Ichor::function<void(Event const * const)> trackFunc;
    };

    class DependencyManager final {
    public:
        DependencyManager() : _memResource(std::pmr::get_default_resource()), _eventMemResource(std::pmr::get_default_resource()) {
        }

        DependencyManager(std::pmr::memory_resource *mainMemoryResource, std::pmr::memory_resource *eventMemoryResource) : _memResource(mainMemoryResource), _eventMemResource(eventMemoryResource) {
        }

        // DANGEROUS COPY, EFFECTIVELY MAKES A NEW MANAGER AND STARTS OVER!!
        // Only implemented so that the manager can be easily used in STL containers before anything is using it.
        [[deprecated("DANGEROUS COPY, EFFECTIVELY MAKES A NEW MANAGER AND STARTS OVER!! The moved-from manager cannot be registered with a CommunicationChannel, or UB occurs.")]]
        DependencyManager(const DependencyManager& other) : _memResource(other._memResource), _eventMemResource(other._eventMemResource) {
            if(other._started) {
                std::terminate();
            }
        };

        // An implementation would be very thread un-safe. Moving the event queue would require no modification by any thread.
        DependencyManager(DependencyManager&&) = delete;


        template<DerivedTemplated<Service> Impl, Derived<IService>... Interfaces>
        requires ImplementsAll<Impl, Interfaces...>
        auto createServiceManager() {
            return createServiceManager<Impl, Interfaces...>(IchorProperties{_memResource});
        }

        template<DerivedTemplated<Service> Impl, Derived<IService>... Interfaces>
        requires ImplementsAll<Impl, Interfaces...>
        auto createServiceManager(IchorProperties&& properties, uint64_t priority = INTERNAL_EVENT_PRIORITY) {
            if constexpr(RequestsDependencies<Impl>) {
                static_assert(!std::is_default_constructible_v<Impl>, "Cannot have a dependencies constructor and a default constructor simultaneously.");
                static_assert(!RequestsProperties<Impl>, "Cannot have a dependencies constructor and a properties constructor simultaneously.");
                auto cmpMgr = DependencyLifecycleManager<Impl>::template create(_logger, "", std::forward<IchorProperties>(properties), this, _memResource, InterfacesList<Interfaces...>);

                if constexpr (sizeof...(Interfaces) > 0) {
                    static_assert(!ListContainsInterface<IFrameworkLogger, Interfaces...>::value, "IFrameworkLogger cannot have any dependencies");
                }

                logAddService<Impl, Interfaces...>();

                for (auto &[key, mgr] : _services) {
                    if (mgr->getServiceState() == ServiceState::ACTIVE) {
                        auto filterProp = mgr->getProperties()->find("Filter");
                        const Filter *filter = nullptr;
                        if (filterProp != end(*mgr->getProperties())) {
                            filter = Ichor::any_cast<Filter * const>(&filterProp->second);
                        }

                        if (filter != nullptr && !filter->compareTo(cmpMgr)) {
                            continue;
                        }

                        cmpMgr->dependencyOnline(mgr.get());
                    }
                }

                bool started = cmpMgr->start();

                for (auto const &[key, registration] : cmpMgr->getDependencyRegistry()->_registrations) {
                    auto const &props = std::get<std::optional<IchorProperties>>(registration);
                    pushEventInternal<DependencyRequestEvent>(cmpMgr->serviceId(), priority, std::get<Dependency>(registration), props.has_value() ? &props.value() : std::optional<IchorProperties const *>{});
                }

                if(!started) {
                    pushEventInternal<StartServiceEvent>(cmpMgr->serviceId(), priority, cmpMgr->serviceId());
                } else {
                    pushEventInternal<DependencyOnlineEvent>(cmpMgr->serviceId(), priority);
                }

                cmpMgr->getService().injectPriority(priority);

                _services.emplace(cmpMgr->serviceId(), cmpMgr);

                return &cmpMgr->getService();
            } else {
                static_assert(!(std::is_default_constructible_v<Impl> && RequestsProperties<Impl>), "Cannot have a properties constructor and a default constructor simultaneously.");
                auto cmpMgr = LifecycleManager<Impl>::template create(_logger, "", std::forward<IchorProperties>(properties), this, _memResource, InterfacesList<Interfaces...>);

                if constexpr (sizeof...(Interfaces) > 0) {
                    if constexpr (ListContainsInterface<IFrameworkLogger, Interfaces...>::value) {
                        _logger = &cmpMgr->getService();
                        _preventEarlyDestructionOfFrameworkLogger = cmpMgr;
                    }
                }

                cmpMgr->getService().injectDependencyManager(this);
                cmpMgr->getService().injectPriority(priority);

                logAddService<Impl, Interfaces...>();

                if(!cmpMgr->start()) {
                    pushEventInternal<StartServiceEvent>(cmpMgr->serviceId(), priority, cmpMgr->serviceId());
                } else {
                    pushEventInternal<DependencyOnlineEvent>(cmpMgr->serviceId(), priority);
                }

                _services.emplace(cmpMgr->serviceId(), cmpMgr);

                return &cmpMgr->getService();
            }
        }

        /// Push event into event loop with specified priority
        /// \tparam EventT Type of event to push, has to derive from Event
        /// \tparam Args auto-deducible arguments for EventT constructor
        /// \param originatingServiceId service that is pushing the event
        /// \param args arguments for EventT constructor
        /// \return event id (can be used in completion/error handlers)
        template <typename EventT, typename... Args>
        requires Derived<EventT, Event>
        uint64_t pushPrioritisedEvent(uint64_t originatingServiceId, uint64_t priority, Args&&... args){
            static_assert(sizeof(EventT) <= 128, "event type cannot be larger than 128 bytes");

            if(_quit.load(std::memory_order_acquire)) {
                ICHOR_LOG_TRACE(_logger, "inserting event of type {} into manager {}, but have to quit", typeName<EventT>(), getId());
                return 0;
            }

            uint64_t eventId = _eventIdCounter.fetch_add(1, std::memory_order_acq_rel);
            _eventQueueMutex.lock();
            _emptyQueue = false;
            [[maybe_unused]] auto it = _eventQueue.emplace(priority, EventStackUniquePtr::create<EventT>(std::forward<uint64_t>(eventId), std::forward<uint64_t>(originatingServiceId), std::forward<uint64_t>(priority), std::forward<Args>(args)...));
            TSAN_ANNOTATE_HAPPENS_BEFORE((void*)&(*it));
            _eventQueueMutex.unlock();
            _wakeUp.notify_all();
            ICHOR_LOG_TRACE(_logger, "inserted event of type {} into manager {}", typeName<EventT>(), getId());
            return eventId;
        }

        /// Push event into event loop with the default priority
        /// \tparam EventT Type of event to push, has to derive from Event
        /// \tparam Args auto-deducible arguments for EventT constructor
        /// \param originatingServiceId service that is pushing the event
        /// \param args arguments for EventT constructor
        /// \return event id (can be used in completion/error handlers)
        template <typename EventT, typename... Args>
        requires Derived<EventT, Event>
        uint64_t pushEvent(uint64_t originatingServiceId, Args&&... args){
            static_assert(sizeof(EventT) <= 128, "event type cannot be larger than 128 bytes");

            if(_quit.load(std::memory_order_acquire)) {
                ICHOR_LOG_TRACE(_logger, "inserting event of type {} into manager {}, but have to quit", typeName<EventT>(), getId());
                return 0;
            }

            uint64_t eventId = _eventIdCounter.fetch_add(1, std::memory_order_acq_rel);
            _eventQueueMutex.lock();
            _emptyQueue = false;
            [[maybe_unused]] auto it = _eventQueue.emplace(INTERNAL_EVENT_PRIORITY, EventStackUniquePtr::create<EventT>(std::forward<uint64_t>(eventId), std::forward<uint64_t>(originatingServiceId), INTERNAL_EVENT_PRIORITY, std::forward<Args>(args)...));
            TSAN_ANNOTATE_HAPPENS_BEFORE((void*)&(*it));
            _eventQueueMutex.unlock();
            _wakeUp.notify_all();
            ICHOR_LOG_TRACE(_logger, "inserted event of type {} into manager {}", typeName<EventT>(), getId());
            return eventId;
        }

        template <typename Interface, typename Impl>
        requires DerivedTemplated<Impl, Service> && ImplementsTrackingHandlers<Impl, Interface>
        [[nodiscard]]
        /// Register handlers for when dependencies get requested/unrequested
        /// \tparam Interface type of interface where dependency is requested for (has to derive from Event)
        /// \tparam Impl type of class registering handler (auto-deducib)
        /// \param serviceId id of service registering handler
        /// \param impl class that is registering handler
        /// \return RAII handler, removes registration upon destruction
        std::unique_ptr<DependencyTrackerRegistration, Deleter> registerDependencyTracker(Impl *impl) {
            auto requestTrackersForType = _dependencyRequestTrackers.find(typeNameHash<Interface>());
            auto undoRequestTrackersForType = _dependencyUndoRequestTrackers.find(typeNameHash<Interface>());

            DependencyTrackerInfo requestInfo{Ichor::function<void(Event const * const)>{[impl](Event const * const evt){ impl->handleDependencyRequest(static_cast<Interface*>(nullptr), static_cast<DependencyRequestEvent const *>(evt)); }, _memResource}};
            DependencyTrackerInfo undoRequestInfo{Ichor::function<void(Event const * const)>{[impl](Event const * const evt){ impl->handleDependencyUndoRequest(static_cast<Interface*>(nullptr), static_cast<DependencyUndoRequestEvent const *>(evt)); }, _memResource}};

            std::pmr::vector<DependencyRequestEvent> requests{getMemoryResource()};
            for(auto const &[key, mgr] : _services) {
                auto const *depRegistry = mgr->getDependencyRegistry();
//                ICHOR_LOG_ERROR(_logger, "register svcId {} dm {}", mgr->serviceId(), _id);

                if(depRegistry == nullptr) {
                    continue;
                }

                for (auto const &[interfaceHash, registration] : depRegistry->_registrations) {
                    if(interfaceHash == typeNameHash<Interface>()) {
                        auto const &props = std::get<std::optional<IchorProperties>>(registration);
                        requests.emplace_back(0, mgr->serviceId(), INTERNAL_EVENT_PRIORITY, std::get<Dependency>(registration), props.has_value() ? &props.value() : std::optional<IchorProperties const *>{});
                    }
                }
            }

            for(const auto& request : requests) {
                requestInfo.trackFunc(&request);
            }

            if(requestTrackersForType == end(_dependencyRequestTrackers)) {
                std::pmr::vector<DependencyTrackerInfo> v{_memResource};
                v.emplace_back(std::move(requestInfo));
                _dependencyRequestTrackers.emplace(typeNameHash<Interface>(), std::move(v));
            } else {
                requestTrackersForType->second.emplace_back(std::move(requestInfo));
            }

            if(undoRequestTrackersForType == end(_dependencyUndoRequestTrackers)) {
                std::pmr::vector<DependencyTrackerInfo> v{_memResource};
                v.emplace_back(std::move(undoRequestInfo));
                _dependencyUndoRequestTrackers.emplace(typeNameHash<Interface>(), std::move(v));
            } else {
                undoRequestTrackersForType->second.emplace_back(std::move(undoRequestInfo));
            }

            return std::unique_ptr<DependencyTrackerRegistration, Deleter>(new (_memResource->allocate(sizeof(DependencyTrackerRegistration))) DependencyTrackerRegistration(this, typeNameHash<Interface>(), impl->getServicePriority()), Deleter{_memResource, sizeof(DependencyTrackerRegistration)});
        }

        template <typename EventT, typename Impl>
        requires Derived<EventT, Event> && ImplementsEventCompletionHandlers<Impl, EventT>
        [[nodiscard]]
        /// Register event error/completion handlers
        /// \tparam EventT type of event (has to derive from Event)
        /// \tparam Impl type of class registering handler (auto-deducible)
        /// \param serviceId id of service registering handler
        /// \param impl class that is registering handler
        /// \return RAII handler, removes registration upon destruction
        std::unique_ptr<EventCompletionHandlerRegistration, Deleter> registerEventCompletionCallbacks(Impl *impl) {
            CallbackKey key{impl->getServiceId(), EventT::TYPE};
            _completionCallbacks.emplace(key, Ichor::function<void(Event const * const)>{[impl](Event const * const evt){ impl->handleCompletion(static_cast<EventT const * const>(evt)); }, _memResource});
            _errorCallbacks.emplace(key, Ichor::function<void(Event const * const)>{[impl](Event const * const evt){ impl->handleError(static_cast<EventT const * const>(evt)); }, _memResource});
            return std::unique_ptr<EventCompletionHandlerRegistration, Deleter>(new (_memResource->allocate(sizeof(EventCompletionHandlerRegistration))) EventCompletionHandlerRegistration(this, key, impl->getServicePriority()), Deleter{_memResource, sizeof(EventCompletionHandlerRegistration)});
        }

        template <typename EventT, typename Impl>
        requires Derived<EventT, Event> && ImplementsEventHandlers<Impl, EventT>
        [[nodiscard]]
        /// Register an event handler
        /// \tparam EventT type of event (has to derive from Event)
        /// \tparam Impl type of class registering handler (auto-deducible)
        /// \param serviceId id of service registering handler
        /// \param impl class that is registering handler
        /// \param targetServiceId optional service id to filter registering for, if empty, receive all events of type EventT
        /// \return RAII handler, removes registration upon destruction
        std::unique_ptr<EventHandlerRegistration, Deleter> registerEventHandler(Impl *impl, std::optional<uint64_t> targetServiceId = {}) {
            auto existingHandlers = _eventCallbacks.find(EventT::TYPE);
            if(existingHandlers == end(_eventCallbacks)) {
                std::pmr::vector<EventCallbackInfo> v{ _memResource };
                v.template emplace_back(EventCallbackInfo{
                        impl->getServiceId(),
                        targetServiceId,
                        Ichor::function<Generator<bool>(Event const * const)>{
                                [impl](Event const *const evt) {
                                    return impl->handleEvent(static_cast<EventT const *const>(evt));
                                },
                                _memResource
                        }
                });
                _eventCallbacks.emplace(EventT::TYPE, std::move(v));
            } else {
                existingHandlers->second.emplace_back(impl->getServiceId(), targetServiceId, Ichor::function<Generator<bool>(Event const *const)>{
                        [impl](Event const *const evt) { return impl->handleEvent(static_cast<EventT const *const>(evt)); }, _memResource});
            }
            return std::unique_ptr<EventHandlerRegistration, Deleter>(new (_memResource->allocate(sizeof(EventHandlerRegistration))) EventHandlerRegistration(this, CallbackKey{impl->getServiceId(), EventT::TYPE}, impl->getServicePriority()), Deleter{_memResource, sizeof(EventHandlerRegistration)});
        }

        template <typename EventT, typename Impl>
        requires Derived<EventT, Event> && ImplementsEventInterceptors<Impl, EventT>
        [[nodiscard]]
        /// Register an event interceptor. If EventT equals Event, intercept all events. Otherwise only intercept given event.
        /// \tparam EventT type of event (has to derive from Event)
        /// \tparam Impl type of class registering handler (auto-deducible)
        /// \param serviceId id of service registering handler
        /// \param impl class that is registering handler
        /// \return RAII handler, removes registration upon destruction
        std::unique_ptr<EventInterceptorRegistration, Deleter> registerEventInterceptor(Impl *impl) {
            uint64_t targetEventId = 0;
            if constexpr (!std::is_same_v<EventT, Event>) {
                targetEventId = EventT::TYPE;
            }
            auto existingHandlers = _eventInterceptors.find(targetEventId);
            if(existingHandlers == end(_eventInterceptors)) {
                std::pmr::vector<EventInterceptInfo> v{_memResource};
                v.template emplace_back(EventInterceptInfo{impl->getServiceId(), targetEventId,
                                   Ichor::function<bool(Event const * const)>{[impl](Event const * const evt){ return impl->preInterceptEvent(static_cast<EventT const * const>(evt)); }, _memResource},
                                   Ichor::function<bool(Event const * const, bool)>{[impl](Event const * const evt, bool processed){ return impl->postInterceptEvent(static_cast<EventT const * const>(evt), processed); }, _memResource}});
                _eventInterceptors.emplace(targetEventId, std::move(v));
            } else {
                existingHandlers->second.template emplace_back(impl->getServiceId(), targetEventId,
                                                      Ichor::function<bool(Event const * const)>{[impl](Event const * const evt){ return impl->preInterceptEvent(static_cast<EventT const * const>(evt)); }, _memResource},
                                                      Ichor::function<bool(Event const * const, bool)>{[impl](Event const * const evt, bool processed){ return impl->postInterceptEvent(static_cast<EventT const * const>(evt), processed); }, _memResource});
            }
            // I think there's a bug in GCC 10.1, where if I don't make this a unique_ptr, the EventHandlerRegistration destructor immediately gets called for some reason.
            // Even if the result is stored in a variable at the caller site.
            return std::unique_ptr<EventInterceptorRegistration, Deleter>(new (_memResource->allocate(sizeof(EventInterceptorRegistration))) EventInterceptorRegistration(this, CallbackKey{impl->getServiceId(), targetEventId}, impl->getServicePriority()), Deleter{_memResource, sizeof(EventInterceptorRegistration)});
        }

        /// Get manager id
        /// \return id
        [[nodiscard]] uint64_t getId() const noexcept {
            return _id;
        }

        [[nodiscard]] std::pmr::memory_resource* getMemoryResource() noexcept {
            return _memResource;
        }

        ///
        /// \return Potentially nullptr
        [[nodiscard]] CommunicationChannel* getCommunicationChannel() const noexcept {
            return _communicationChannel;
        }

        /// Get framework logger
        /// \return Potentially nullptr
        [[nodiscard]] IFrameworkLogger* getLogger() const noexcept {
            return _logger;
        }

        [[nodiscard]] bool isRunning() const noexcept {
            return _started;
        };

        template <typename Interface>
        [[nodiscard]] std::pmr::vector<Interface*> getStartedServices() noexcept {
            std::pmr::vector<Interface*> ret{_memResource};
            ret.reserve(_services.size());
            for(auto &[key, svc] : _services) {
                if(svc->getServiceState() != ServiceState::ACTIVE) {
                    continue;
                }

                for(auto &iface : svc->getInterfaces()) {
                    if(iface.interfaceNameHash == typeNameHash<Interface>()) {
                        ret.push_back(reinterpret_cast<Interface*>(svc->getServiceAsInterfacePointer()));
                        break;
                    }
                }
            }

            return ret;
        }

        // This waits on all events done processing, rather than the event queue being empty.
        void waitForEmptyQueue() {
            std::shared_lock lck(_eventQueueMutex);
            _wakeUp.wait_for(lck, std::chrono::milliseconds(1), [this] { return _emptyQueue || _quit.load(std::memory_order_acquire); });
        }

        [[nodiscard]] std::optional<std::string_view> getImplementationNameFor(uint64_t serviceId) const noexcept;

        void start();

    private:
        template <typename EventT>
        requires Derived<EventT, Event>
        void handleEventError(EventT const * const evt) const {
            if(evt->originatingService == 0) {
                return;
            }

            auto service = _services.find(evt->originatingService);
            if(service == end(_services) || service->second->getServiceState() != ServiceState::ACTIVE) {
                return;
            }

            auto callback = _errorCallbacks.find(CallbackKey{evt->originatingService, EventT::TYPE});
            if(callback == end(_errorCallbacks)) {
                return;
            }

            callback->second(evt);
        }

        template <typename Impl, typename Interface1, typename Interface2, typename... Interfaces>
        void logAddService() {
            if(_logger != nullptr && _logger->getLogLevel() <= LogLevel::DEBUG) {
                fmt::memory_buffer out;
                fmt::format_to(out, "added ServiceManager<{}, {}, ", typeName<Interface1>(), typeName<Interface2>());
                (fmt::format_to(out, "{}, ", typeName<Interfaces>()), ...);
                fmt::format_to(out, "{}>", typeName<Impl>());
                ICHOR_LOG_DEBUG(_logger, "{}", out.data());
            }
        }

        template <typename Impl, typename Interface>
        void logAddService() {
            if(_logger != nullptr && _logger->getLogLevel() <= LogLevel::DEBUG) {
                ICHOR_LOG_DEBUG(_logger, "added ServiceManager<{}, {}>", typeName<Interface>(), typeName<Impl>());
            }
        }

        template <typename Impl>
        void logAddService() {
            if(_logger != nullptr && _logger->getLogLevel() <= LogLevel::DEBUG) {
                ICHOR_LOG_DEBUG(_logger, "added ServiceManager<{}>", typeName<Impl>());
            }
        }

        void handleEventCompletion(Event const * const evt) const;

        [[nodiscard]] uint32_t broadcastEvent(Event const * const evt);

        void setCommunicationChannel(CommunicationChannel *channel);

        template <typename EventT, typename... Args>
        requires Derived<EventT, Event>
        uint64_t pushEventInternal(uint64_t originatingServiceId, uint64_t priority, Args&&... args) {
            static_assert(sizeof(EventT) <= 128, "event type cannot be larger than 128 bytes");

            uint64_t eventId = _eventIdCounter.fetch_add(1, std::memory_order_acq_rel);
            _eventQueueMutex.lock();
            _emptyQueue = false;
            [[maybe_unused]] auto it = _eventQueue.emplace(priority, EventStackUniquePtr::create<EventT>(std::forward<uint64_t>(eventId), std::forward<uint64_t>(originatingServiceId), std::forward<uint64_t>(priority), std::forward<Args>(args)...));
            TSAN_ANNOTATE_HAPPENS_BEFORE((void*)&(*it));
            _eventQueueMutex.unlock();
            _wakeUp.notify_all();
            return eventId;
        }

        std::pmr::memory_resource *_memResource;
        std::pmr::memory_resource *_eventMemResource; // cannot be shared with _memResource, as that would introduce threading issues
        std::pmr::unordered_map<uint64_t, std::shared_ptr<ILifecycleManager>> _services{_memResource}; // key = service id
        std::pmr::unordered_map<uint64_t, std::pmr::vector<DependencyTrackerInfo>> _dependencyRequestTrackers{_memResource}; // key = interface name hash
        std::pmr::unordered_map<uint64_t, std::pmr::vector<DependencyTrackerInfo>> _dependencyUndoRequestTrackers{_memResource}; // key = interface name hash
        std::pmr::unordered_map<CallbackKey, Ichor::function<void(Event const * const)>> _completionCallbacks{_memResource}; // key = listening service id + event type
        std::pmr::unordered_map<CallbackKey, Ichor::function<void(Event const * const)>> _errorCallbacks{_memResource}; // key = listening service id + event type
        std::pmr::unordered_map<uint64_t, std::pmr::vector<EventCallbackInfo>> _eventCallbacks{_memResource}; // key = event id
        std::pmr::unordered_map<uint64_t, std::pmr::vector<EventInterceptInfo>> _eventInterceptors{_memResource}; // key = event id
        IFrameworkLogger *_logger{nullptr};
        std::shared_ptr<ILifecycleManager> _preventEarlyDestructionOfFrameworkLogger{nullptr};
        std::pmr::multimap<uint64_t, EventStackUniquePtr> _eventQueue{_eventMemResource};
        RealtimeReadWriteMutex _eventQueueMutex{};
        ConditionVariableAny<RealtimeReadWriteMutex> _wakeUp{_eventQueueMutex};
        std::atomic<uint64_t> _eventIdCounter{0};
        std::atomic<bool> _quit{false};
        std::atomic<bool> _started{false};
        bool _emptyQueue{false}; // only true when all events are done processing, as opposed to having an empty _eventQueue. The latter can be empty before processing due to the usage of extract()
        CommunicationChannel *_communicationChannel{nullptr};
        uint64_t _id{_managerIdCounter++};
        static std::atomic<uint64_t> _managerIdCounter;

        friend class EventCompletionHandlerRegistration;
        friend class CommunicationChannel;
    };
}
