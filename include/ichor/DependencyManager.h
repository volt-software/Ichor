#pragma once

// used to solve some nasty circular includes
#define ICHOR_DEPENDENCY_MANAGER 1

#include <vector>
#include <unordered_map>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <ichor/interfaces/IFrameworkLogger.h>
#include <ichor/dependency_management/Service.h>
#include <ichor/dependency_management/ILifecycleManager.h>
#include <ichor/events/InternalEvents.h>
#include <ichor/coroutines/IGenerator.h>
#include <ichor/coroutines/AsyncGenerator.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <ichor/Callbacks.h>
#include <ichor/Filter.h>
#include <ichor/dependency_management/DependencyRegistrations.h>
#include <ichor/dependency_management/ConstructorInjectionService.h>
#include <ichor/event_queues/IEventQueue.h>

using namespace std::chrono_literals;

namespace Ichor {
    class CommunicationChannel;

    struct DependencyTrackerInfo final {
        explicit DependencyTrackerInfo(std::function<void(Event const &)> _trackFunc) noexcept : trackFunc(std::move(_trackFunc)) {}
        std::function<void(Event const &)> trackFunc;
    };

    // Moved here from InternalEvents.h to prevent circular includes
    /// Used to prevent modifying the _services container while iterating over it through f.e. DependencyOnline()
    struct InsertServiceEvent final : public Event {
        InsertServiceEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, std::unique_ptr<ILifecycleManager> _mgr) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority), mgr(std::move(_mgr)) {}
        ~InsertServiceEvent() final = default;

        std::unique_ptr<ILifecycleManager> mgr;
        static constexpr uint64_t TYPE = typeNameHash<InsertServiceEvent>();
        static constexpr std::string_view NAME = typeName<InsertServiceEvent>();
    };

    struct EventWaiter final {
        explicit EventWaiter(uint64_t _waitingSvcId, uint64_t _eventType) : waitingSvcId(_waitingSvcId), eventType(_eventType) {
            events.emplace_back(_eventType, std::make_unique<AsyncManualResetEvent>());
        }
        EventWaiter(const EventWaiter &) = delete;
        EventWaiter(EventWaiter &&o) noexcept = default;
        std::vector<std::pair<uint64_t, std::unique_ptr<AsyncManualResetEvent>>> events{};
        uint64_t waitingSvcId;
        uint64_t eventType;
        uint32_t count{1}; // default of 1, to be decremented in event completion/error
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
            if(other._started.load(std::memory_order_acquire)) [[unlikely]] {
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
            return internalCreateServiceManager<Impl, Interfaces...>(Properties{});
        }

        template<DerivedTemplated<Service> Impl, typename... Interfaces>
        // msvc compiler bug, see https://developercommunity.visualstudio.com/t/c20-Friend-definition-of-class-with-re/10197302
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires ImplementsAll<Impl, Interfaces...>
#endif
        Impl* createServiceManager(Properties&& properties, uint64_t priority = INTERNAL_EVENT_PRIORITY) {
            return internalCreateServiceManager<Impl, Interfaces...>(std::move(properties), priority);
        }

        template<typename Impl, typename... Interfaces>
        // msvc compiler bug, see https://developercommunity.visualstudio.com/t/c20-Friend-definition-of-class-with-re/10197302
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires ImplementsAll<Impl, Interfaces...>
#endif
        IService* createServiceManager() {
            return createConstructorInjectorServiceManager<Impl, Interfaces...>(Properties{}, INTERNAL_EVENT_PRIORITY);
        }

        template<typename Impl, typename... Interfaces>
        // msvc compiler bug, see https://developercommunity.visualstudio.com/t/c20-Friend-definition-of-class-with-re/10197302
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires ImplementsAll<Impl, Interfaces...>
#endif
        IService* createServiceManager(Properties&& properties, uint64_t priority = INTERNAL_EVENT_PRIORITY) {
            return createConstructorInjectorServiceManager<Impl, Interfaces...>(std::move(properties), priority);
        }

        template<typename Impl, typename... Interfaces>
        // msvc compiler bug, see https://developercommunity.visualstudio.com/t/c20-Friend-definition-of-class-with-re/10197302
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires ImplementsAll<Impl, Interfaces...>
#endif
        IService* createConstructorInjectorServiceManager(Properties&& properties, uint64_t priority = INTERNAL_EVENT_PRIORITY) {
            return internalCreateServiceManager<ConstructorInjectionService<Impl>, Interfaces...>(std::move(properties), priority);
        }

        template<typename Impl, typename... Interfaces>
        Impl* internalCreateServiceManager(Properties&& properties, uint64_t priority = INTERNAL_EVENT_PRIORITY) {
#ifdef ICHOR_USE_HARDENING
            if(_started.load(std::memory_order_acquire) && this != Detail::_local_dm) [[unlikely]] { // are we on the right thread?
                std::terminate();
            }
#endif
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

                        auto depIts = cmpMgr->interestedInDependency(mgr.get(), true);

                        if(depIts.empty()) {
                            continue;
                        }

                        auto gen = cmpMgr->dependencyOnline(mgr.get(), std::move(depIts));
                        auto it = gen.begin();

                        if(!it.get_finished()) {
                            _scopedGenerators.emplace(it.get_promise_id(), std::make_unique<AsyncGenerator<StartBehaviour>>(std::move(gen)));
                            // create new event that will be inserted upon finish of coroutine in ContinuableStartEvent
                            _scopedEvents.emplace(it.get_promise_id(), std::make_shared<DependencyOnlineEvent>(getNextEventId(), cmpMgr->serviceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY));
                        } else if(it.get_value() == StartBehaviour::STARTED) {
                            pushPrioritisedEvent<DependencyOnlineEvent>(cmpMgr->serviceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY);
                        }
                    }
                }

                for (auto const &[key, registration] : cmpMgr->getDependencyRegistry()->_registrations) {
                    auto const &props = std::get<std::optional<Properties>>(registration);
                    pushPrioritisedEvent<DependencyRequestEvent>(cmpMgr->serviceId(), priority, std::get<Dependency>(registration), props.has_value() ? &props.value() : std::optional<Properties const *>{});
                }

                auto event_priority = std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, priority);
                pushPrioritisedEvent<StartServiceEvent>(cmpMgr->serviceId(), event_priority, cmpMgr->serviceId());

                cmpMgr->getService().injectPriority(priority);

                if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                    if (_services.contains(cmpMgr->serviceId())) [[unlikely]] {
                        std::terminate();
                    }
                }

                Impl* impl = &cmpMgr->getService();
                // Can't directly emplace mgr into _services as that would result into modifying the container while iterating.
                pushPrioritisedEvent<InsertServiceEvent>(cmpMgr->serviceId(), INTERNAL_INSERT_SERVICE_EVENT_PRIORITY, std::move(cmpMgr));

                return impl;
            } else {
                static_assert(!(std::is_default_constructible_v<Impl> && RequestsProperties<Impl>), "Cannot have a properties constructor and a default constructor simultaneously.");
                auto cmpMgr = LifecycleManager<Impl, Interfaces...>::template create<>(std::forward<Properties>(properties), this, InterfacesList<Interfaces...>);

                if constexpr (sizeof...(Interfaces) > 0) {
                    if constexpr (ListContainsInterface<IFrameworkLogger, Interfaces...>::value) {
                        static_assert(!IsConstructorInjector<Impl>, "Framework loggers cannot use constructor injection");
                        _logger = cmpMgr->getService().getImplementation();
                    }
                }

                cmpMgr->getService().injectDependencyManager(this);
                cmpMgr->getService().injectPriority(priority);

                logAddService<Impl, Interfaces...>(cmpMgr->serviceId());

                auto event_priority = std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, priority);
                pushPrioritisedEvent<StartServiceEvent>(cmpMgr->serviceId(), event_priority, cmpMgr->serviceId());

                if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                    if (_services.contains(cmpMgr->serviceId())) [[unlikely]] {
                        std::terminate();
                    }
                }

                Impl* impl = &cmpMgr->getService();
                pushPrioritisedEvent<InsertServiceEvent>(cmpMgr->serviceId(), INTERNAL_INSERT_SERVICE_EVENT_PRIORITY, std::move(cmpMgr));

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
        uint64_t pushPrioritisedEvent(uint64_t originatingServiceId, uint64_t priority, Args&&... args) {
            static_assert(EventT::TYPE == typeNameHash<EventT>(), "Event typeNameHash wrong");
            static_assert(EventT::NAME == typeName<EventT>(), "Event typeName wrong");

            // TODO hardening
//#ifdef ICHOR_USE_HARDENING
//            if(originatingServiceId != 0 && _services.find(originatingServiceId) == _services.end()) [[unlikely]] {
//                std::terminate();
//            }
//#endif

            uint64_t eventId = getNextEventId();
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
        uint64_t pushEvent(uint64_t originatingServiceId, Args&&... args) {
            static_assert(EventT::TYPE == typeNameHash<EventT>(), "Event typeNameHash wrong");
            static_assert(EventT::NAME == typeName<EventT>(), "Event typeName wrong");

            // TODO hardening
//#ifdef ICHOR_USE_HARDENING
//            if(originatingServiceId != 0 && _services.find(originatingServiceId) == _services.end()) [[unlikely]] {
//                std::terminate();
//            }
//#endif

            uint64_t eventId = getNextEventId();
            _eventQueue->pushEvent(INTERNAL_EVENT_PRIORITY, std::unique_ptr<Event>{new EventT(std::forward<uint64_t>(eventId), std::forward<uint64_t>(originatingServiceId), INTERNAL_EVENT_PRIORITY, std::forward<Args>(args)...)});
//            ICHOR_LOG_TRACE(_logger, "inserted event of type {} into manager {}", typeName<EventT>(), getId());
            return eventId;
        }

        /// Push event into event loop with the default priority asynchronously
        /// \tparam EventT Type of event to push, has to derive from Event
        /// \tparam Args auto-deducible arguments for EventT constructor
        /// \param originatingServiceId service that is pushing the event
        /// \param priority priority of event
        /// \param coalesce on true: if event of given type and originatingServiceId is already being awaited on somewhere, await on that one instead of creating a new event
        /// \param args arguments for EventT constructor
        /// \return event id (can be used in completion/error handlers)
        template <typename EventT, typename... Args>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires Derived<EventT, Event>
#endif
        AsyncGenerator<void> pushPrioritisedEventAsync(uint64_t originatingServiceId, uint64_t priority, bool coalesce, Args&&... args) {
#ifdef ICHOR_USE_HARDENING
            if(this != Detail::_local_dm) [[unlikely]] { // are we on the right thread?
                std::terminate();
            }
#endif
            static_assert(EventT::TYPE == typeNameHash<EventT>(), "Event typeNameHash wrong");
            static_assert(EventT::NAME == typeName<EventT>(), "Event typeName wrong");
            static_assert(!std::is_same_v<EventT, QuitEvent>, "QuitEvent cannot be used in an async manner");
            static_assert(!std::is_same_v<EventT, RemoveCompletionCallbacksEvent>, "RemoveCompletionCallbacksEvent cannot be used in an async manner");
            static_assert(!std::is_same_v<EventT, RemoveEventHandlerEvent>, "RemoveEventHandlerEvent cannot be used in an async manner");
            static_assert(!std::is_same_v<EventT, RemoveEventInterceptorEvent>, "RemoveEventInterceptorEvent cannot be used in an async manner");
            static_assert(!std::is_same_v<EventT, RemoveTrackerEvent>, "RemoveTrackerEvent cannot be used in an async manner");
            static_assert(!std::is_same_v<EventT, ContinuableEvent>, "ContinuableEvent cannot be used in an async manner");
            static_assert(!std::is_same_v<EventT, ContinuableStartEvent>, "ContinuableStartEvent cannot be used in an async manner");

#ifdef ICHOR_USE_HARDENING
            if(originatingServiceId != 0 && _services.find(originatingServiceId) == _services.end()) [[unlikely]] {
                std::terminate();
            }
#endif

            if(coalesce) {
                auto waitingIt = std::find_if(_eventWaiters.begin(), _eventWaiters.end(),
                                              [originatingServiceId](const std::pair<const uint64_t, EventWaiter> &waiter) {
                                                  return waiter.second.waitingSvcId == originatingServiceId && waiter.second.eventType == EventT::TYPE;
                                              });

                if (waitingIt != _eventWaiters.end()) {
                    auto &e = waitingIt->second.events.emplace_back(EventT::TYPE, std::make_unique<AsyncManualResetEvent>());
                    co_await *e.second.get();
                    co_return;
                }
            }

            uint64_t eventId = getNextEventId();
            _eventQueue->pushEvent(priority, std::unique_ptr<Event>{new EventT(std::forward<uint64_t>(eventId), std::forward<uint64_t>(originatingServiceId), std::forward<uint64_t>(priority), std::forward<Args>(args)...)});
            auto it = _eventWaiters.emplace(eventId, EventWaiter(originatingServiceId, EventT::TYPE));
            INTERNAL_DEBUG("pushPrioritisedEventAsync {}:{} {} waiting {} {}", eventId, typeName<EventT>(), originatingServiceId, it.first->second.count, it.first->second.events.size());
            co_await *it.first->second.events.begin()->second.get();
            co_return;
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
#ifdef ICHOR_USE_HARDENING
            if(this != Detail::_local_dm) [[unlikely]] { // are we on the right thread?
                std::terminate();
            }
#endif
            auto requestTrackersForType = _dependencyRequestTrackers.find(typeNameHash<Interface>());
            auto undoRequestTrackersForType = _dependencyUndoRequestTrackers.find(typeNameHash<Interface>());

            DependencyTrackerInfo requestInfo{std::function<void(Event const &)>{[impl](Event const &evt) {
                impl->handleDependencyRequest(static_cast<Interface*>(nullptr), static_cast<DependencyRequestEvent const &>(evt));
            }}};
            DependencyTrackerInfo undoRequestInfo{std::function<void(Event const &)>{[impl](Event const &evt) {
                impl->handleDependencyUndoRequest(static_cast<Interface*>(nullptr), static_cast<DependencyUndoRequestEvent const &>(evt));
            }}};

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

            return DependencyTrackerRegistration(this, impl->getServiceId(), typeNameHash<Interface>(), impl->getServicePriority());
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
            static_assert(!std::is_same_v<EventT, QuitEvent>, "QuitEvent cannot be used for completion callbacks");
            static_assert(!std::is_same_v<EventT, RemoveCompletionCallbacksEvent>, "RemoveCompletionCallbacksEvent cannot be used for completion callbacks");
            static_assert(!std::is_same_v<EventT, RemoveEventHandlerEvent>, "RemoveEventHandlerEvent cannot be used for completion callbacks");
            static_assert(!std::is_same_v<EventT, RemoveEventInterceptorEvent>, "RemoveEventInterceptorEvent cannot be used for completion callbacks");
            static_assert(!std::is_same_v<EventT, RemoveTrackerEvent>, "RemoveTrackerEvent cannot be used for completion callbacks");
            static_assert(!std::is_same_v<EventT, ContinuableEvent>, "ContinuableEvent cannot be used for completion callbacks");
            static_assert(!std::is_same_v<EventT, ContinuableStartEvent>, "ContinuableStartEvent cannot be used for completion callbacks");
            static_assert(!std::is_same_v<EventT, InsertServiceEvent>, "InsertServiceEvent cannot be used for completion callbacks");

#ifdef ICHOR_USE_HARDENING
            if(this != Detail::_local_dm) [[unlikely]] { // are we on the right thread?
                std::terminate();
            }
#endif
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
#ifdef ICHOR_USE_HARDENING
            if(this != Detail::_local_dm) [[unlikely]] { // are we on the right thread?
                std::terminate();
            }
#endif
            auto existingHandlers = _eventCallbacks.find(EventT::TYPE);
            if(existingHandlers == end(_eventCallbacks)) {
                std::vector<EventCallbackInfo> v{};
                v.template emplace_back<>(EventCallbackInfo{
                        impl->getServiceId(),
                        targetServiceId,
                        std::function<AsyncGenerator<IchorBehaviour>(Event const &)>{
                            [impl](Event const &evt) { return impl->handleEvent(static_cast<EventT const &>(evt)); }
                        }
                });
                _eventCallbacks.emplace(EventT::TYPE, std::move(v));
            } else {
                existingHandlers->second.emplace_back(EventCallbackInfo{
                    impl->getServiceId(),
                    targetServiceId,
                    std::function<AsyncGenerator<IchorBehaviour>(Event const &)>{
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
#ifdef ICHOR_USE_HARDENING
            if(this != Detail::_local_dm) [[unlikely]] { // are we on the right thread?
                std::terminate();
            }
#endif
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
            return EventInterceptorRegistration(this, CallbackKey{impl->getServiceId(), targetEventId}, impl->getServicePriority());
        }

        /// Get manager id. Thread-safe.
        /// \return id
        [[nodiscard]] uint64_t getId() const noexcept {
            return _id;
        }

        /// Get communication channel associated with this manager.
        /// \return Potentially nullptr
        [[nodiscard]] CommunicationChannel* getCommunicationChannel() const noexcept {
            return _communicationChannel;
        }

        /// Get framework logger
        /// \return Potentially nullptr
        [[nodiscard]] IFrameworkLogger* getLogger() const noexcept {
#ifdef ICHOR_USE_HARDENING
            if(this != Detail::_local_dm) [[unlikely]] { // are we on the right thread?
                std::terminate();
            }
#endif
            return _logger;
        }

        [[nodiscard]] bool isRunning() const noexcept {
            return _started.load(std::memory_order_acquire);
        };

        /// Get number of services known to this dependency mananger
        /// \return number
        [[nodiscard]] uint64_t getServiceCount() const noexcept {
#ifdef ICHOR_USE_HARDENING
            if(this != Detail::_local_dm) [[unlikely]] { // are we on the right thread?
                std::terminate();
            }
#endif
            return _services.size();
        }

        /// Get service by local ID
        /// \param id service id
        /// \return optional
        [[nodiscard]] std::optional<const IService*> getService(uint64_t id) const noexcept {
#ifdef ICHOR_USE_HARDENING
            if(this != Detail::_local_dm) [[unlikely]] { // are we on the right thread?
                std::terminate();
            }
#endif
            auto svc = _services.find(id);

            if(svc == _services.end()) {
                return {};
            }

            return svc->second->getIService();
        }
        /// Get service by global ID, much slower than getting by local ID
        /// \param id service uuid
        /// \return optional
        [[nodiscard]] std::optional<const IService*> getService(sole::uuid id) const noexcept {
#ifdef ICHOR_USE_HARDENING
            if(this != Detail::_local_dm) [[unlikely]] { // are we on the right thread?
                std::terminate();
            }
#endif
            auto svc = std::find_if(_services.begin(), _services.end(), [&id](const std::pair<const uint64_t, std::unique_ptr<ILifecycleManager>> &svcPair) {
                return svcPair.second->getIService()->getServiceGid() == id;
            });

            if(svc == _services.end()) {
                return {};
            }

            return svc->second->getIService();
        }

        template <typename Interface>
        [[nodiscard]] std::vector<Interface*> getStartedServices() noexcept {
#ifdef ICHOR_USE_HARDENING
            if(this != Detail::_local_dm) [[unlikely]] { // are we on the right thread?
                std::terminate();
            }
#endif
            std::vector<Interface*> ret{};
            ret.reserve(_services.size());
            std::function<void(void*, IService*)> f{[&ret](void *svc2, IService * /*isvc*/){ ret.push_back(reinterpret_cast<Interface*>(svc2)); }};
            for(auto &[key, svc] : _services) {
                if(svc->getServiceState() != ServiceState::ACTIVE) {
                    continue;
                }
                svc->insertSelfInto(typeNameHash<Interface>(), 0, f);
                svc->getDependees().erase(0);
            }

            return ret;
        }

        template <typename Interface>
        /// Get all services by given template interface type, regardless of state
        /// \tparam Interface interface to search for
        /// \return list of found services
        [[nodiscard]] std::vector<Interface*> getAllServicesOfType() noexcept {
#ifdef ICHOR_USE_HARDENING
            if(this != Detail::_local_dm) [[unlikely]] { // are we on the right thread?
                std::terminate();
            }
#endif
            std::vector<Interface*> ret{};
            ret.reserve(_services.size());
            std::function<void(void*, IService*)> f{[&ret](void *svc2, IService * /*isvc*/){ ret.push_back(reinterpret_cast<Interface*>(svc2)); }};
            for(auto &[key, svc] : _services) {
                auto intf = std::find_if(svc->getInterfaces().begin(), svc->getInterfaces().end(), [](const Dependency &dep) {
                    return dep.interfaceNameHash == typeNameHash<Interface>();
                });
                if(intf == svc->getInterfaces().end()) {
                    continue;
                }
                svc->insertSelfInto(typeNameHash<Interface>(), 0, f);
                svc->getDependees().erase(0);
            }

            return ret;
        }

        /// Returns a list of currently known services and their status.
        /// Do not use in coroutines or other threads
        /// NOT thread-safe!!
        /// \return map of [serviceId, service]
        [[nodiscard]] unordered_map<uint64_t, IService const *> getServiceInfo() const noexcept;

        // Mainly useful for tests
        void runForOrQueueEmpty(std::chrono::milliseconds ms = 100ms) const noexcept;

        [[nodiscard]] std::optional<std::string_view> getImplementationNameFor(uint64_t serviceId) const noexcept;

        [[nodiscard]] uint64_t getNextEventId() noexcept {
            return _eventIdCounter.fetch_add(1, std::memory_order_acq_rel);
        }

        [[nodiscard]] IService const * getIServiceForImplementation(void const *ptr) const noexcept {
            for(auto &[k, svc] : _services) {
                if(svc->getTypedServicePtr() == ptr) {
                    return svc->getIService();
                }
            }

            std::terminate();
        }

        [[nodiscard]] AsyncGenerator<void> waitForStarted(IService *svc) {
            co_await waitForService(svc->getServiceId(), DependencyOnlineEvent::TYPE).begin();
        }

    private:
        template <typename EventT>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires Derived<EventT, Event>
#endif
        void handleEventError(EventT const &evt) {
            auto waitingIt = _eventWaiters.find(evt.id);
            if(waitingIt != end(_eventWaiters)) {
                waitingIt->second.count--;
                INTERNAL_DEBUG("handleEventError {}:{} {} waiting {} {}", evt.id, evt.name, evt.originatingService, waitingIt->second.count, waitingIt->second.events.size());
#ifdef ICHOR_USE_HARDENING
                if(waitingIt->second.count == std::numeric_limits<decltype(waitingIt->second.count)>::max()) [[unlikely]] {
                    std::terminate();
                }
#endif
                if(waitingIt->second.count == 0) {
                    for(auto &asyncEvt : waitingIt->second.events) {
                        asyncEvt.second->set();
                    }
                    _eventWaiters.erase(waitingIt);
                }
            }

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
        [[nodiscard]] bool existingCoroutineFor(uint64_t serviceId) const noexcept;
        /// Coroutine based method to wait for a service to have finished with either DependencyOfflineEvent or StopServiceEvent
        /// \param serviceId
        /// \param eventType
        /// \return
        AsyncGenerator<void> waitForService(uint64_t serviceId, uint64_t eventType) noexcept;
        /// Counterpart for waitForService, runs all waiting coroutines for specified event
        /// \param serviceId
        /// \param eventType
        /// \param eventName
        /// \return
        bool finishWaitingService(uint64_t serviceId, uint64_t eventType, [[maybe_unused]] std::string_view eventName) noexcept;

        unordered_map<uint64_t, std::unique_ptr<ILifecycleManager>> _services{}; // key = service id
        unordered_map<uint64_t, std::vector<DependencyTrackerInfo>> _dependencyRequestTrackers{}; // key = interface name hash
        unordered_map<uint64_t, std::vector<DependencyTrackerInfo>> _dependencyUndoRequestTrackers{}; // key = interface name hash
        unordered_map<CallbackKey, std::function<void(Event const &)>> _completionCallbacks{}; // key = listening service id + event type
        unordered_map<CallbackKey, std::function<void(Event const &)>> _errorCallbacks{}; // key = listening service id + event type
        unordered_map<uint64_t, std::vector<EventCallbackInfo>> _eventCallbacks{}; // key = event id
        unordered_map<uint64_t, std::vector<EventInterceptInfo>> _eventInterceptors{}; // key = event id
        unordered_map<uint64_t, std::unique_ptr<IGenerator>> _scopedGenerators{}; // key = promise id
        unordered_map<uint64_t, std::shared_ptr<Event>> _scopedEvents{}; // key = promise id
        unordered_map<uint64_t, EventWaiter> _eventWaiters{}; // key = event id
        unordered_map<uint64_t, EventWaiter> _dependencyWaiters{}; // key = event id
        IEventQueue *_eventQueue;
        IFrameworkLogger *_logger{nullptr};
        std::atomic<uint64_t> _eventIdCounter{0};
        std::atomic<bool> _started{false};
        CommunicationChannel *_communicationChannel{nullptr};
        uint64_t _id{_managerIdCounter++};
        static std::atomic<uint64_t> _managerIdCounter;

        friend class IEventQueue;
        friend class ILifecycleManager;
        friend class EventCompletionHandlerRegistration;
        friend class CommunicationChannel;
    };


    namespace Detail {
        // Used internally to insert events where passing around the DM isn't feasible
        // Only use if you know for sure you're on the correct thread.
        thread_local extern DependencyManager *_local_dm;
    }

    [[nodiscard]] DependencyManager& GetThreadLocalManager() noexcept;
}

#include <ichor/coroutines/AsyncGeneratorDetail.h>
