#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <ichor/interfaces/IFrameworkLogger.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include <ichor/events/InternalEvents.h>
#include <ichor/coroutines//IGenerator.h>
#include <ichor/Callbacks.h>
#include <ichor/Filter.h>
#include <ichor/DependencyRegistrations.h>
#include <ichor/event_queues/IEventQueue.h>

using namespace std::chrono_literals;

namespace Ichor {
    class CommunicationChannel;

    template <typename T>
    class AsyncGenerator;

    struct DependencyTrackerInfo final {
        explicit DependencyTrackerInfo(std::function<void(Event const &)> _trackFunc) noexcept : trackFunc(std::move(_trackFunc)) {}
        std::function<void(Event const &)> trackFunc;
    };

    class DependencyManager final {
    private:
        explicit DependencyManager(IEventQueue *eventQueue) : _eventQueue(eventQueue) {

        }
    public:
        // DANGEROUS COPY, EFFECTIVELY MAKES A NEW MANAGER AND STARTS OVER!!
        // Only implemented so that the manager can be easily used in STL containers before anything is using it.
        [[deprecated("DANGEROUS COPY, EFFECTIVELY MAKES A NEW MANAGER AND STARTS OVER!! The moved-from manager cannot be registered with a CommunicationChannel, or UB occurs.")]]
        DependencyManager(const DependencyManager& other) : _eventQueue(other._eventQueue) {
            if(other._started) {
                std::terminate();
            }
        };

        // An implementation would be very thread un-safe. Moving the event queue would require no modification by any thread.
        DependencyManager(DependencyManager&&) = delete;

        ~DependencyManager() {
            stop();
        }


        template<DerivedTemplated<Service> Impl, typename... Interfaces>
        // msvc compiler bug, see https://developercommunity.visualstudio.com/t/c20-Friend-definition-of-class-with-re/10197302
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires ImplementsAll<Impl, Interfaces...>
#endif
        Impl* createServiceManager() {
            return createServiceManager<Impl, Interfaces...>(Properties{});
        }

        template<DerivedTemplated<Service> Impl, typename... Interfaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires ImplementsAll<Impl, Interfaces...>
#endif
        Impl* createServiceManager(Properties&& properties, uint64_t priority = INTERNAL_EVENT_PRIORITY) {
            if constexpr(RequestsDependencies<Impl>) {
                static_assert(!std::is_default_constructible_v<Impl>, "Cannot have a dependencies constructor and a default constructor simultaneously.");
                static_assert(!RequestsProperties<Impl>, "Cannot have a dependencies constructor and a properties constructor simultaneously.");
                auto cmpMgr = DependencyLifecycleManager<Impl>::template create<>(std::forward<Properties>(properties), this, InterfacesList<Interfaces...>);

                if constexpr (sizeof...(Interfaces) > 0) {
                    static_assert(!ListContainsInterface<IFrameworkLogger, Interfaces...>::value, "IFrameworkLogger cannot have any dependencies");
                }

                logAddService<Impl, Interfaces...>(cmpMgr->serviceId());

                for (auto &[key, mgr] : _services) {
                    if (mgr->getServiceState() == ServiceState::ACTIVE) {
                        auto const filterProp = mgr->getProperties().find("Filter");
                        const Filter *filter = nullptr;
                        if (filterProp != cend(mgr->getProperties())) {
                            filter = Ichor::any_cast<Filter * const>(&filterProp->second);
                        }

                        if (filter != nullptr && !filter->compareTo(*cmpMgr.get())) {
                            continue;
                        }

                        cmpMgr->dependencyOnline(mgr.get());
                    }
                }

                auto started = cmpMgr->start();

                for (auto const &[key, registration] : cmpMgr->getDependencyRegistry()->_registrations) {
                    auto const &props = std::get<std::optional<Properties>>(registration);
                    pushPrioritisedEvent<DependencyRequestEvent>(cmpMgr->serviceId(), priority, std::get<Dependency>(registration), props.has_value() ? &props.value() : std::optional<Properties const *>{});
                }

                auto event_priority = std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, priority);
                if(started == StartBehaviour::FAILED_AND_RETRY) {
                    pushPrioritisedEvent<StartServiceEvent>(cmpMgr->serviceId(), event_priority, cmpMgr->serviceId());
                } else if(started == StartBehaviour::SUCCEEDED) {
                    pushPrioritisedEvent<DependencyOnlineEvent>(cmpMgr->serviceId(), event_priority);
                }

                cmpMgr->getService().injectPriority(priority);

                if constexpr (DO_INTERNAL_DEBUG) {
                    if (_services.contains(cmpMgr->serviceId())) {
                        std::terminate();
                    }
                }

                Impl* impl = &cmpMgr->getService();
                _services.emplace(cmpMgr->serviceId(), std::move(cmpMgr));

                return impl;
            } else {
                static_assert(!(std::is_default_constructible_v<Impl> && RequestsProperties<Impl>), "Cannot have a properties constructor and a default constructor simultaneously.");
                auto cmpMgr = LifecycleManager<Impl, Interfaces...>::template create<>(std::forward<Properties>(properties), this, InterfacesList<Interfaces...>);

                if constexpr (sizeof...(Interfaces) > 0) {
                    if constexpr (ListContainsInterface<IFrameworkLogger, Interfaces...>::value) {
                        _logger = &cmpMgr->getService();
                    }
                }

                cmpMgr->getService().injectDependencyManager(this);
                cmpMgr->getService().injectPriority(priority);

                logAddService<Impl, Interfaces...>(cmpMgr->serviceId());

                auto started = cmpMgr->start();
                auto event_priority = std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, priority);
                if(started == StartBehaviour::FAILED_AND_RETRY) {
                    pushPrioritisedEvent<StartServiceEvent>(cmpMgr->serviceId(), event_priority, cmpMgr->serviceId());
                } else if(started == StartBehaviour::SUCCEEDED) {
                    pushPrioritisedEvent<DependencyOnlineEvent>(cmpMgr->serviceId(), event_priority);
                }

                if constexpr (DO_INTERNAL_DEBUG) {
                    if (_services.contains(cmpMgr->serviceId())) {
                        std::terminate();
                    }
                }

                Impl* impl = &cmpMgr->getService();
                _services.emplace(cmpMgr->serviceId(), std::move(cmpMgr));

                return impl;
            }
        }

        /// Push event into event loop with specified priority
        /// \tparam EventT Type of event to push, has to derive from Event
        /// \tparam Args auto-deducible arguments for EventT constructor
        /// \param originatingServiceId service that is pushing the event
        /// \param args arguments for EventT constructor
        /// \return event id (can be used in completion/error handlers)
        template <typename EventT, typename... Args>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires Derived<EventT, Event>
#endif
        uint64_t pushPrioritisedEvent(uint64_t originatingServiceId, uint64_t priority, Args&&... args){
            uint64_t eventId = _eventIdCounter.fetch_add(1, std::memory_order_acq_rel);
            _eventQueue->pushEvent(priority, std::unique_ptr<Event>{new EventT(std::forward<uint64_t>(eventId), std::forward<uint64_t>(originatingServiceId), std::forward<uint64_t>(priority), std::forward<Args>(args)...)});
//            ICHOR_LOG_TRACE(_logger, "inserted event of type {} into manager {}", typeName<EventT>(), getId());
            return eventId;
        }

        /// Push event into event loop with the default priority
        /// \tparam EventT Type of event to push, has to derive from Event
        /// \tparam Args auto-deducible arguments for EventT constructor
        /// \param originatingServiceId service that is pushing the event
        /// \param args arguments for EventT constructor
        /// \return event id (can be used in completion/error handlers)
        template <typename EventT, typename... Args>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires Derived<EventT, Event>
#endif
        uint64_t pushEvent(uint64_t originatingServiceId, Args&&... args){
            uint64_t eventId = _eventIdCounter.fetch_add(1, std::memory_order_acq_rel);
            _eventQueue->pushEvent(INTERNAL_EVENT_PRIORITY, std::unique_ptr<Event>{new EventT(std::forward<uint64_t>(eventId), std::forward<uint64_t>(originatingServiceId), INTERNAL_EVENT_PRIORITY, std::forward<Args>(args)...)});
//            ICHOR_LOG_TRACE(_logger, "inserted event of type {} into manager {}", typeName<EventT>(), getId());
            return eventId;
        }

        template <typename Interface, typename Impl>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires DerivedTemplated<Impl, Service> && ImplementsTrackingHandlers<Impl, Interface>
#endif
        [[nodiscard]]
        /// Register handlers for when dependencies get requested/unrequested
        /// \tparam Interface type of interface where dependency is requested for (has to derive from Event)
        /// \tparam Impl type of class registering handler (auto-deducib)
        /// \param serviceId id of service registering handler
        /// \param impl class that is registering handler
        /// \return RAII handler, removes registration upon destruction
        DependencyTrackerRegistration registerDependencyTracker(Impl *impl) {
            auto requestTrackersForType = _dependencyRequestTrackers.find(typeNameHash<Interface>());
            auto undoRequestTrackersForType = _dependencyUndoRequestTrackers.find(typeNameHash<Interface>());

            DependencyTrackerInfo requestInfo{std::function<void(Event const &)>{[impl](Event const &evt){ impl->handleDependencyRequest(static_cast<Interface*>(nullptr), static_cast<DependencyRequestEvent const &>(evt)); }}};
            DependencyTrackerInfo undoRequestInfo{std::function<void(Event const &)>{[impl](Event const &evt){ impl->handleDependencyUndoRequest(static_cast<Interface*>(nullptr), static_cast<DependencyUndoRequestEvent const &>(evt)); }}};
            
            std::vector<DependencyRequestEvent> requests{};
            for(auto const &[key, mgr] : _services) {
                auto const *depRegistry = mgr->getDependencyRegistry();
//                ICHOR_LOG_ERROR(_logger, "register svcId {} dm {}", mgr->serviceId(), _id);

                if(depRegistry == nullptr) {
                    continue;
                }

                for (auto const &[interfaceHash, registration] : depRegistry->_registrations) {
                    if(interfaceHash == typeNameHash<Interface>()) {
                        auto const &props = std::get<std::optional<Properties>>(registration);
                        requests.emplace_back(0, mgr->serviceId(), INTERNAL_EVENT_PRIORITY, std::get<Dependency>(registration), props.has_value() ? &props.value() : std::optional<Properties const *>{});
                    }
                }
            }

            for(const auto& request : requests) {
                requestInfo.trackFunc(request);
            }

            if(requestTrackersForType == end(_dependencyRequestTrackers)) {
                std::vector<DependencyTrackerInfo> v{};
                v.emplace_back(std::move(requestInfo));
                _dependencyRequestTrackers.emplace(typeNameHash<Interface>(), std::move(v));
            } else {
                requestTrackersForType->second.emplace_back(std::move(requestInfo));
            }

            if(undoRequestTrackersForType == end(_dependencyUndoRequestTrackers)) {
                std::vector<DependencyTrackerInfo> v{};
                v.emplace_back(std::move(undoRequestInfo));
                _dependencyUndoRequestTrackers.emplace(typeNameHash<Interface>(), std::move(v));
            } else {
                undoRequestTrackersForType->second.emplace_back(std::move(undoRequestInfo));
            }

            return DependencyTrackerRegistration(this, typeNameHash<Interface>(), impl->getServicePriority());
        }

        template <typename EventT, typename Impl>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires Derived<EventT, Event> && ImplementsEventCompletionHandlers<Impl, EventT>
#endif
        [[nodiscard]]
        /// Register event error/completion handlers
        /// \tparam EventT type of event (has to derive from Event)
        /// \tparam Impl type of class registering handler (auto-deducible)
        /// \param serviceId id of service registering handler
        /// \param impl class that is registering handler
        /// \return RAII handler, removes registration upon destruction
        EventCompletionHandlerRegistration registerEventCompletionCallbacks(Impl *impl) {
            CallbackKey key{impl->getServiceId(), EventT::TYPE};
            _completionCallbacks.emplace(key, std::function<void(Event const &)>{[impl](Event const &evt){ impl->handleCompletion(static_cast<EventT const &>(evt)); }});
            _errorCallbacks.emplace(key, std::function<void(Event const &)>{[impl](Event const &evt){ impl->handleError(static_cast<EventT const &>(evt)); }});
            return EventCompletionHandlerRegistration(this, key, impl->getServicePriority());
        }

        template <typename EventT, typename Impl>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires Derived<EventT, Event> && ImplementsEventHandlers<Impl, EventT>
#endif
        [[nodiscard]]
        /// Register an event handler
        /// \tparam EventT type of event (has to derive from Event)
        /// \tparam Impl type of class registering handler (auto-deducible)
        /// \param serviceId id of service registering handler
        /// \param impl class that is registering handler
        /// \param targetServiceId optional service id to filter registering for, if empty, receive all events of type EventT
        /// \return RAII handler, removes registration upon destruction
        EventHandlerRegistration registerEventHandler(Impl *impl, std::optional<uint64_t> targetServiceId = {}) {
            auto existingHandlers = _eventCallbacks.find(EventT::TYPE);
            if(existingHandlers == end(_eventCallbacks)) {
                std::vector<EventCallbackInfo> v{};
                v.template emplace_back<>(EventCallbackInfo{
                        impl->getServiceId(),
                        targetServiceId,
                        std::function<AsyncGenerator<void>(Event const &)>{
                            [impl](Event const &evt) { return impl->handleEvent(static_cast<EventT const &>(evt)); }
                        }
                });
                _eventCallbacks.emplace(EventT::TYPE, std::move(v));
            } else {
                existingHandlers->second.emplace_back(EventCallbackInfo{
                    impl->getServiceId(),
                    targetServiceId,
                    std::function<AsyncGenerator<void>(Event const &)>{
                        [impl](Event const &evt) { return impl->handleEvent(static_cast<EventT const &>(evt)); }
                    }
                });
            }
            return EventHandlerRegistration(this, CallbackKey{impl->getServiceId(), EventT::TYPE}, impl->getServicePriority());
        }

        template <typename EventT, typename Impl>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires Derived<EventT, Event> && ImplementsEventInterceptors<Impl, EventT>
#endif
        [[nodiscard]]
        /// Register an event interceptor. If EventT equals Event, intercept all events. Otherwise only intercept given event.
        /// \tparam EventT type of event (has to derive from Event)
        /// \tparam Impl type of class registering handler (auto-deducible)
        /// \param serviceId id of service registering handler
        /// \param impl class that is registering handler
        /// \return RAII handler, removes registration upon destruction
        EventInterceptorRegistration registerEventInterceptor(Impl *impl) {
            uint64_t targetEventId = 0;
            if constexpr (!std::is_same_v<EventT, Event>) {
                targetEventId = EventT::TYPE;
            }
            auto existingHandlers = _eventInterceptors.find(targetEventId);
            if(existingHandlers == end(_eventInterceptors)) {
                std::vector<EventInterceptInfo> v{};
                v.template emplace_back<>(EventInterceptInfo{impl->getServiceId(), targetEventId,
                                   std::function<bool(Event const &)>{[impl](Event const &evt){ return impl->preInterceptEvent(static_cast<EventT const &>(evt)); }},
                                   std::function<void(Event const &, bool)>{[impl](Event const &evt, bool processed){ impl->postInterceptEvent(static_cast<EventT const &>(evt), processed); }}});
                _eventInterceptors.emplace(targetEventId, std::move(v));
            } else {
                existingHandlers->second.template emplace_back<>(EventInterceptInfo{impl->getServiceId(), targetEventId,
                                                      std::function<bool(Event const &)>{[impl](Event const &evt){ return impl->preInterceptEvent(static_cast<EventT const &>(evt)); }},
                                                      std::function<void(Event const &, bool)>{[impl](Event const &evt, bool processed){ impl->postInterceptEvent(static_cast<EventT const &>(evt), processed); }}});
            }
            // I think there's a bug in GCC 10.1, where if I don't make this a unique_ptr, the EventHandlerRegistration destructor immediately gets called for some reason.
            // Even if the result is stored in a variable at the caller site.
            return EventInterceptorRegistration(this, CallbackKey{impl->getServiceId(), targetEventId}, impl->getServicePriority());
        }

        /// Get manager id
        /// \return id
        [[nodiscard]] uint64_t getId() const noexcept {
            return _id;
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

        uint64_t getServiceCount() const noexcept {
            return _services.size();
        }

        template <typename Interface>
        [[nodiscard]] std::vector<Interface*> getStartedServices() noexcept {
            std::vector<Interface*> ret{};
            ret.reserve(_services.size());
            for(auto &[key, svc] : _services) {
                if(svc->getServiceState() != ServiceState::ACTIVE) {
                    continue;
                }
                std::function<void(void*, IService*)> f{[&ret](void *svc2, IService *isvc){ ret.push_back(reinterpret_cast<Interface*>(svc2)); }};
                svc->insertSelfInto(typeNameHash<Interface>(), 0, f);
                svc->getDependees().erase(0);
            }

            return ret;
        }

        // Mainly useful for tests
        void runForOrQueueEmpty(std::chrono::milliseconds ms = 100ms) const noexcept;

        [[nodiscard]] std::optional<std::string_view> getImplementationNameFor(uint64_t serviceId) const noexcept;

        [[nodiscard]] uint64_t getNextEventId() noexcept;

    private:
        template <typename EventT>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires Derived<EventT, Event>
#endif
        void handleEventError(EventT const &evt) const {
            if(evt.originatingService == 0) {
                return;
            }

            auto service = _services.find(evt.originatingService);
            if(service == end(_services) || service->second->getServiceState() != ServiceState::ACTIVE) {
                return;
            }

            auto callback = _errorCallbacks.find(CallbackKey{evt.originatingService, EventT::TYPE});
            if(callback == end(_errorCallbacks)) {
                return;
            }

            callback->second(evt);
        }

        template <typename Impl, typename Interface1, typename Interface2, typename... Interfaces>
        void logAddService(uint64_t id) {
            if(_logger != nullptr && _logger->getLogLevel() <= LogLevel::LOG_DEBUG) {
                fmt::memory_buffer out;
                fmt::format_to(std::back_inserter(out), "added ServiceManager<{}, {}, ", typeName<Interface1>(), typeName<Interface2>());
                (fmt::format_to(std::back_inserter(out), "{}, ", typeName<Interfaces>()), ...);
                fmt::format_to(std::back_inserter(out), "{}>", typeName<Impl>());
                ICHOR_LOG_DEBUG(_logger, "{} {}", out.data(), id);
            }
        }

        template <typename Impl, typename Interface>
        void logAddService(uint64_t id) {
            if(_logger != nullptr && _logger->getLogLevel() <= LogLevel::LOG_DEBUG) {
                ICHOR_LOG_DEBUG(_logger, "added ServiceManager<{}, {}> {}", typeName<Interface>(), typeName<Impl>(), id);
            }
        }

        template <typename Impl>
        void logAddService(uint64_t id) {
            if(_logger != nullptr && _logger->getLogLevel() <= LogLevel::LOG_DEBUG) {
                ICHOR_LOG_DEBUG(_logger, "added ServiceManager<{}> {}", typeName<Impl>(), id);
            }
        }

        void handleEventCompletion(Event const &evt);
        [[nodiscard]] uint64_t broadcastEvent(std::shared_ptr<Event> &evt);
        void setCommunicationChannel(CommunicationChannel *channel);
        void start();
        void processEvent(std::unique_ptr<Event> &&evt);
        void stop();

        unordered_map<uint64_t, std::unique_ptr<ILifecycleManager>> _services{}; // key = service id
        unordered_map<uint64_t, std::vector<DependencyTrackerInfo>> _dependencyRequestTrackers{}; // key = interface name hash
        unordered_map<uint64_t, std::vector<DependencyTrackerInfo>> _dependencyUndoRequestTrackers{}; // key = interface name hash
        unordered_map<CallbackKey, std::function<void(Event const &)>> _completionCallbacks{}; // key = listening service id + event type
        unordered_map<CallbackKey, std::function<void(Event const &)>> _errorCallbacks{}; // key = listening service id + event type
        unordered_map<uint64_t, std::vector<EventCallbackInfo>> _eventCallbacks{}; // key = event id
        unordered_map<uint64_t, std::vector<EventInterceptInfo>> _eventInterceptors{}; // key = event id
        unordered_map<uint64_t, std::unique_ptr<IGenerator>> _scopedGenerators{}; // key = promise id
        unordered_map<uint64_t, std::shared_ptr<Event>> _scopedEvents{}; // key = promise id
        IEventQueue *_eventQueue;
        IFrameworkLogger *_logger{nullptr};
        std::atomic<uint64_t> _eventIdCounter{0};
        std::atomic<bool> _started{false};
        CommunicationChannel *_communicationChannel{nullptr};
        uint64_t _id{_managerIdCounter++};
        static std::atomic<uint64_t> _managerIdCounter;

        friend class IEventQueue;
        friend class EventCompletionHandlerRegistration;
        friend class CommunicationChannel;
    };


    namespace Detail {
        // Used internally to insert events where passing around the DM isn't feasible
        // Only use if you know for sure you're on the correct thread.
        thread_local extern DependencyManager *_local_dm;
    }
}

#include <ichor/coroutines/AsyncGenerator.h>
