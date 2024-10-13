#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <atomic>
#include <span>
#include <ichor/interfaces/IFrameworkLogger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/events/InternalEvents.h>
#include <ichor/coroutines/IGenerator.h>
#include <ichor/coroutines/AsyncGenerator.h>
#include <ichor/dependency_management/ILifecycleManager.h>
#include <ichor/dependency_management/DependencyLifecycleManager.h>
#include <ichor/dependency_management/LifecycleManager.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <ichor/Callbacks.h>
#include <ichor/Filter.h>
#include <ichor/dependency_management/DependencyRegistrations.h>
#include <ichor/dependency_management/ConstructorInjectionService.h>
#include <ichor/dependency_management/DependencyTrackers.h>
#include <ichor/event_queues/IEventQueue.h>
#include <ichor/stl/ServiceProtectedPointer.h>

using namespace std::chrono_literals;

namespace Ichor {
    class CommunicationChannel;

    // Moved here from InternalEvents.h to prevent circular includes
    /// Used to prevent modifying the _services container while iterating over it through f.e. DependencyOnline()
    struct InsertServiceEvent final : public Event {
        InsertServiceEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority, std::unique_ptr<ILifecycleManager> _mgr) noexcept : Event(_id, _originatingService, _priority), mgr(std::move(_mgr)) {}
        ~InsertServiceEvent() final = default;

        [[nodiscard]] std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] NameHashType get_type() const noexcept final {
            return TYPE;
        }

        std::unique_ptr<ILifecycleManager> mgr;
        static constexpr NameHashType TYPE = typeNameHash<InsertServiceEvent>();
        static constexpr std::string_view NAME = typeName<InsertServiceEvent>();
    };

    struct EventWaiter final {
        explicit EventWaiter(ServiceIdType _waitingSvcId, uint64_t _eventType) : waitingSvcId(_waitingSvcId), eventType(_eventType) {
            events.emplace_back(_eventType, std::make_unique<AsyncManualResetEvent>());
        }
        EventWaiter(const EventWaiter &) = delete;
        EventWaiter(EventWaiter &&o) noexcept = default;
        EventWaiter& operator=(EventWaiter const &) = delete;
        EventWaiter& operator=(EventWaiter&&) = default;

        std::vector<std::pair<uint64_t, std::unique_ptr<AsyncManualResetEvent>>> events{};
        ServiceIdType waitingSvcId;
        uint64_t eventType;
    };

    class [[nodiscard]] DependencyManager final {
    private:
        explicit DependencyManager(IEventQueue *eventQueue);
    public:
        /// DANGEROUS COPY, EFFECTIVELY MAKES A NEW MANAGER AND STARTS OVER!!
        /// Only implemented so that the manager can be easily used in STL containers before anything is using it.
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

        template<DerivedTemplated<AdvancedService> Impl, typename... Interfaces>
        // msvc compiler bug, see https://developercommunity.visualstudio.com/t/c20-Friend-definition-of-class-with-re/10197302
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires ImplementsAll<Impl, Interfaces...>
#endif
        ServiceProtectedPointer<Impl> createServiceManager() {
            return internalCreateServiceManager<Impl, Impl, Interfaces...>(Properties{});
        }

        template<DerivedTemplated<AdvancedService> Impl, typename... Interfaces>
        // msvc compiler bug, see https://developercommunity.visualstudio.com/t/c20-Friend-definition-of-class-with-re/10197302
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires ImplementsAll<Impl, Interfaces...>
#endif
        ServiceProtectedPointer<Impl> createServiceManager(Properties&& properties, uint64_t priority = INTERNAL_EVENT_PRIORITY) {
            return internalCreateServiceManager<Impl, Impl, Interfaces...>(std::forward<Properties>(properties), priority);
        }

        template<typename Impl, typename... Interfaces>
        // msvc compiler bug, see https://developercommunity.visualstudio.com/t/c20-Friend-definition-of-class-with-re/10197302
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires ImplementsAll<Impl, Interfaces...>
#endif
        ServiceProtectedPointer<IService> createServiceManager() {
            return createConstructorInjectorServiceManager<Impl, Interfaces...>(Properties{}, INTERNAL_EVENT_PRIORITY);
        }

        template<typename Impl, typename... Interfaces>
        // msvc compiler bug, see https://developercommunity.visualstudio.com/t/c20-Friend-definition-of-class-with-re/10197302
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires ImplementsAll<Impl, Interfaces...>
#endif
        ServiceProtectedPointer<IService> createServiceManager(Properties&& properties, uint64_t priority = INTERNAL_EVENT_PRIORITY) {
            return createConstructorInjectorServiceManager<Impl, Interfaces...>(std::move(properties), priority);
        }

        template<typename Impl, typename... Interfaces>
        // msvc compiler bug, see https://developercommunity.visualstudio.com/t/c20-Friend-definition-of-class-with-re/10197302
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires ImplementsAll<Impl, Interfaces...>
#endif
        ServiceProtectedPointer<IService> createConstructorInjectorServiceManager(Properties&& properties, uint64_t priority = INTERNAL_EVENT_PRIORITY) {
            return internalCreateServiceManager<ConstructorInjectionService<Impl>, IService, Interfaces...>(std::move(properties), priority);
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
        Task<void> pushPrioritisedEventAsync(ServiceIdType originatingServiceId, uint64_t priority, bool coalesce, Args&&... args) {
            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (this != Detail::_local_dm) [[unlikely]] {
                    ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
                    std::terminate();
                }
            }
            static_assert(EventT::TYPE == typeNameHash<EventT>(), "Event typeNameHash wrong");
            static_assert(EventT::NAME == typeName<EventT>(), "Event typeName wrong");
            static_assert(!std::is_same_v<EventT, QuitEvent>, "QuitEvent cannot be used in an async manner");
            static_assert(!std::is_same_v<EventT, RemoveEventHandlerEvent>, "RemoveEventHandlerEvent cannot be used in an async manner");
            static_assert(!std::is_same_v<EventT, RemoveEventInterceptorEvent>, "RemoveEventInterceptorEvent cannot be used in an async manner");
            static_assert(!std::is_same_v<EventT, AddTrackerEvent>, "AddTrackerEvent cannot be used in an async manner");
            static_assert(!std::is_same_v<EventT, RemoveTrackerEvent>, "RemoveTrackerEvent cannot be used in an async manner");
            static_assert(!std::is_same_v<EventT, ContinuableEvent>, "ContinuableEvent cannot be used in an async manner");
            static_assert(!std::is_same_v<EventT, ContinuableStartEvent>, "ContinuableStartEvent cannot be used in an async manner");
            static_assert(!std::is_same_v<EventT, DependencyRequestEvent>, "DependencyRequestEvent cannot be used in an async manner");
            static_assert(!std::is_same_v<EventT, DependencyUndoRequestEvent>, "DependencyUndoRequestEvent cannot be used in an async manner");
            static_assert(!std::is_same_v<EventT, DependencyOnlineEvent>, "DependencyOnlineEvent cannot be used in an async manner");
            static_assert(!std::is_same_v<EventT, DependencyOfflineEvent>, "DependencyOfflineEvent cannot be used in an async manner");

            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (originatingServiceId != 0 && _services.find(originatingServiceId) == _services.end()) [[unlikely]] {
                    ICHOR_EMERGENCY_LOG2(_logger, "Trying to push event with unknown service id {}.", originatingServiceId);
                    std::terminate();
                }
            }

            if(coalesce) {
                auto const waitingIt = std::find_if(_eventWaiters.begin(), _eventWaiters.end(),
                                              [originatingServiceId](const auto &waiter) {
                                                  return waiter.second.waitingSvcId == originatingServiceId && waiter.second.eventType == EventT::TYPE;
                                              });

                if (waitingIt != _eventWaiters.end()) {
                    auto &e = waitingIt->second.events.emplace_back(EventT::TYPE, std::make_unique<AsyncManualResetEvent>());
                    co_await *e.second.get();
                    co_return;
                }
            }

            uint64_t eventId = _eventQueue->getNextEventId();
            _eventQueue->pushEventInternal(priority, std::unique_ptr<Event>{new EventT(std::forward<uint64_t>(eventId), std::forward<ServiceIdType>(originatingServiceId), std::forward<uint64_t>(priority), std::forward<Args>(args)...)});
            auto it = _eventWaiters.emplace(eventId, EventWaiter(originatingServiceId, EventT::TYPE));
            INTERNAL_DEBUG("pushPrioritisedEventAsync {}:{} {} events.size {}", eventId, typeName<EventT>(), originatingServiceId, it.first->second.events.size());
            co_await *it.first->second.events.begin()->second.get();
            co_return;
        }

        template <typename Interface, typename Impl>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
        requires ImplementsTrackingHandlers<Impl, Interface>
#endif
        [[nodiscard]]
        /// Register handlers for when dependencies get requested/unrequested
        /// \tparam Interface type of interface where dependency is requested for (has to derive from Event)
        /// \tparam Impl type of class registering handler (auto-deducible)
        /// \param serviceId id of service registering handler
        /// \param impl class that is registering handler
        /// \return RAII handler, removes registration upon destruction
        DependencyTrackerRegistration registerDependencyTracker(Impl *impl, IService *self) {
            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (this != Detail::_local_dm) [[unlikely]] {
                    ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
                    std::terminate();
                }

                if(self->getServiceState() < ServiceState::STARTING || self->getServiceState() > ServiceState::ACTIVE) {
                    ICHOR_EMERGENCY_LOG1(_logger, "Cannot register tracker unless service is starting/started. Did you accidentally try registering inside an AdvancedService constructor? Try calling it in start() instead.");
                    std::terminate();
                }
            }
            auto requestTrackersForType = _dependencyRequestTrackers.find(typeNameHash<Interface>());

            DependencyTrackerInfo requestInfo{self->getServiceId(), std::function<AsyncGenerator<IchorBehaviour>(Event const &)>{[impl](Event const &evt) -> AsyncGenerator<IchorBehaviour> {
                return impl->handleDependencyRequest(AlwaysNull<Interface*>(), static_cast<DependencyRequestEvent const &>(evt));
            }}, std::function<AsyncGenerator<IchorBehaviour>(Event const &)>{[impl](Event const &evt) -> AsyncGenerator<IchorBehaviour> {
                return impl->handleDependencyUndoRequest(AlwaysNull<Interface*>(), static_cast<DependencyUndoRequestEvent const &>(evt));
            }}};

            if(requestTrackersForType == end(_dependencyRequestTrackers)) {
                std::vector<DependencyTrackerInfo> v{};
                v.emplace_back(std::move(requestInfo));
                _dependencyRequestTrackers.emplace(DependencyTrackerKey{typeName<Interface>(), typeNameHash<Interface>()}, std::move(v));
            } else {
                requestTrackersForType->second.emplace_back(std::move(requestInfo));
            }

            _eventQueue->pushPrioritisedEvent<AddTrackerEvent>(self->getServiceId(), std::min(INTERNAL_INSERT_SERVICE_EVENT_PRIORITY, self->getServicePriority()), typeNameHash<Interface>());

            return DependencyTrackerRegistration(self->getServiceId(), typeNameHash<Interface>(), self->getServicePriority());
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
        EventHandlerRegistration registerEventHandler(Impl *impl, IService *self, tl::optional<ServiceIdType> targetServiceId = {}) {
            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (this != Detail::_local_dm) [[unlikely]] {
                    ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
                    std::terminate();
                }
            }

            auto existingHandlers = _eventCallbacks.find(EventT::TYPE);
            if(existingHandlers == end(_eventCallbacks)) {
                std::vector<EventCallbackInfo> v{};
                v.template emplace_back<>(EventCallbackInfo{
                        self->getServiceId(),
                        targetServiceId,
                        std::function<AsyncGenerator<IchorBehaviour>(Event const &)>{
                            [impl](Event const &evt) { return impl->handleEvent(static_cast<EventT const &>(evt)); }
                        }
                });
                _eventCallbacks.emplace(EventT::TYPE, std::move(v));
            } else {
                existingHandlers->second.emplace_back(EventCallbackInfo{
                    self->getServiceId(),
                    targetServiceId,
                    std::function<AsyncGenerator<IchorBehaviour>(Event const &)>{
                        [impl](Event const &evt) { return impl->handleEvent(static_cast<EventT const &>(evt)); }
                    }
                });
            }
            return EventHandlerRegistration(CallbackKey{self->getServiceId(), EventT::TYPE}, self->getServicePriority());
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
        EventInterceptorRegistration registerEventInterceptor(Impl *impl, IService *self) {
            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (this != Detail::_local_dm) [[unlikely]] {
                    ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
                    std::terminate();
                }
            }

            uint64_t targetEventId = 0;
            if constexpr (!std::is_same_v<EventT, Event>) {
                targetEventId = EventT::TYPE;
            }
            auto existingHandlers = _eventInterceptors.find(targetEventId);
            if(existingHandlers == end(_eventInterceptors)) {
                std::vector<EventInterceptInfo> v{};
                v.template emplace_back<>(EventInterceptInfo{self->getServiceId(), targetEventId,
                                   std::function<bool(Event const &)>{[impl](Event const &evt){ return impl->preInterceptEvent(static_cast<EventT const &>(evt)); }},
                                   std::function<void(Event const &, bool)>{[impl](Event const &evt, bool processed){ impl->postInterceptEvent(static_cast<EventT const &>(evt), processed); }}});
                _eventInterceptors.emplace(targetEventId, std::move(v));
            } else {
                existingHandlers->second.template emplace_back<>(EventInterceptInfo{self->getServiceId(), targetEventId,
                                                      std::function<bool(Event const &)>{[impl](Event const &evt){ return impl->preInterceptEvent(static_cast<EventT const &>(evt)); }},
                                                      std::function<void(Event const &, bool)>{[impl](Event const &evt, bool processed){ impl->postInterceptEvent(static_cast<EventT const &>(evt), processed); }}});
            }
            return EventInterceptorRegistration(CallbackKey{self->getServiceId(), targetEventId}, self->getServicePriority());
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
            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (this != Detail::_local_dm) [[unlikely]] {
                    ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
                    std::terminate();
                }
            }
            return _logger;
        }

        [[nodiscard]] bool isRunning() const noexcept {
            return _started.load(std::memory_order_acquire);
        };

        /// Get number of services known to this dependency mananger
        /// \return number
        [[nodiscard]] uint64_t getServiceCount() const noexcept {
            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (this != Detail::_local_dm) [[unlikely]] {
                    ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
                    std::terminate();
                }
            }
            return _services.size();
        }

        /// Get IService by local ID
        /// \param id service id
        /// \return optional
        [[nodiscard]] tl::optional<NeverNull<const IService*>> getIService(ServiceIdType id) const noexcept;

        /// Get IService by global ID, much slower than getting by local ID
        /// \param id service uuid
        /// \return optional
        [[nodiscard]] tl::optional<NeverNull<const IService*>> getIService(sole::uuid id) const noexcept;


        template <typename Interface>
        [[nodiscard]] tl::optional<std::pair<Interface*, IService*>> getService(ServiceIdType id) const noexcept {
            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (this != Detail::_local_dm) [[unlikely]] {
                    ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
                    std::terminate();
                }
            }

            auto svc = _services.find(id);

            if(svc == _services.end()) {
                return {};
            }

            auto intf = std::find_if(svc->second->getInterfaces().begin(), svc->second->getInterfaces().end(), [](Dependency const &d) {
                return d.interfaceNameHash == typeNameHash<Interface>();
            });

            if(intf == svc->second->getInterfaces().end()) {
                return {};
            }
            Interface* ret{};
            IService* retIsvc{};
            std::function<void(NeverNull<void*>, IService&)> f{[&ret, &retIsvc](NeverNull<void*> svc2, IService& isvc){ ret = reinterpret_cast<Interface*>(svc2.get()); retIsvc = &isvc; }};
            svc->second->insertSelfInto(typeNameHash<Interface>(), 0, f);
            svc->second->getDependees().erase(0);

            return std::pair<Interface*, IService*>(ret, retIsvc);
        }

        template <typename Interface>
        [[nodiscard]] std::vector<NeverNull<Interface*>> getStartedServices() noexcept {
            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (this != Detail::_local_dm) [[unlikely]] {
                    ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
                    std::terminate();
                }
            }
            std::vector<NeverNull<Interface*>> ret{};
            std::function<void(NeverNull<void*>, IService&)> f{[&ret](NeverNull<void*> svc2, IService& /*isvc*/){ ret.push_back(reinterpret_cast<Interface*>(svc2.get())); }};
            for(auto &[key, svc] : _services) {
                if(svc->getServiceState() != ServiceState::ACTIVE) {
                    continue;
                }
                svc->insertSelfInto(typeNameHash<Interface>(), 0, f);
                svc->getDependees().erase(0);
            }

            return ret;
        }

        /// Get all services by given template interface type, regardless of state
        /// Do not use in coroutines or other threads.
        /// Not thread-safe.
        /// \tparam Interface interface to search for
        /// \return list of found services
        template <typename Interface>
        [[nodiscard]] std::vector<std::pair<Interface&, IService&>> getAllServicesOfType() noexcept {
            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (this != Detail::_local_dm) [[unlikely]] {
                    ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
                    std::terminate();
                }
            }
            std::vector<std::pair<Interface&, IService&>> ret{};
            std::function<void(NeverNull<void*>, IService&)> f{[&ret](NeverNull<void*> svc2, IService & isvc){ ret.emplace_back(*reinterpret_cast<Interface*>(svc2.get()), isvc); }};
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

        [[nodiscard]] std::vector<Dependency> getDependencyRequestsForService(ServiceIdType svcId) const noexcept;
        [[nodiscard]] std::vector<NeverNull<IService const *>> getDependentsForService(ServiceIdType svcId) const noexcept;
        [[nodiscard]] std::span<Dependency const> getProvidedInterfacesForService(ServiceIdType svcId) const noexcept;
        [[nodiscard]] std::vector<DependencyTrackerKey> getTrackersForService(ServiceIdType svcId) const noexcept;

        /// Returns a list of currently known services and their status.
        /// Do not use in coroutines or other threads.
        /// Not thread-safe.
        /// \return map of [serviceId, service]
        [[nodiscard]] unordered_map<ServiceIdType, NeverNull<IService const *>> getAllServices() const noexcept;

        /// Blocks until the queue is empty or the specified timeout has passed.
        /// Mainly useful for tests
        void runForOrQueueEmpty(std::chrono::milliseconds ms = 100ms) const noexcept;

        /// Convenience function. Gets the class name of the implementation for given serviceId.
        /// \param serviceId
        /// \return nullopt if serviceId does not exist, name of implementation class otherwise
        [[nodiscard]] tl::optional<std::string_view> getImplementationNameFor(ServiceIdType serviceId) const noexcept;

        /// Gets the associated event queue
        [[nodiscard]] IEventQueue& getEventQueue() const noexcept;

        /// Async method to wait until a service is started
        /// \param svc
        /// \return immediately return void if service is already started, await if not, WaitError if quitting
        [[nodiscard]] Task<tl::expected<void, WaitError>> waitForServiceStarted(NeverNull<IService*> svc);

        /// Async method to wait until a service is stopped
        /// \param svc
        /// \return immediately return void if service is already stopped, await if not, WaitError if quitting
        [[nodiscard]] Task<tl::expected<void, WaitError>> waitForServiceStopped(NeverNull<IService*> svc);

    private:
        template<typename Impl, typename ReturnImpl, typename... Interfaces>
        ServiceProtectedPointer<ReturnImpl> internalCreateServiceManager(Properties&& properties, uint64_t priority = INTERNAL_EVENT_PRIORITY) {
            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (_started.load(std::memory_order_acquire) && this != Detail::_local_dm) [[unlikely]] {
                    ICHOR_EMERGENCY_LOG1(_logger, "Function called from wrong thread.");
                    std::terminate();
                }
            }
            if constexpr(RequestsDependencies<Impl>) {
                static_assert(!std::is_default_constructible_v<Impl>, "Cannot have a dependencies constructor and a default constructor simultaneously.");
                static_assert(!RequestsProperties<Impl>, "Cannot have a dependencies constructor and a properties constructor simultaneously.");
                auto cmpMgr = Detail::DependencyLifecycleManager<Impl>::template create<>(std::forward<Properties>(properties), InterfacesList<Interfaces...>);
                auto serviceId = cmpMgr->serviceId();

                if constexpr (sizeof...(Interfaces) > 0) {
                    static_assert(!ListContainsInterface<IFrameworkLogger, Interfaces...>::value, "IFrameworkLogger cannot have any dependencies");
                }

                logAddService<Impl, Interfaces...>(serviceId);

                auto event_priority = std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, priority);

                cmpMgr->getService().setServicePriority(priority);

                if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                    if (_services.contains(serviceId)) [[unlikely]] {
                        ICHOR_EMERGENCY_LOG1(_logger, "Service id already exists. Bug in Ichor?");
                        std::terminate();
                    }
                }

                Impl* impl = &cmpMgr->getService();
                DependencyRegister const *reg = cmpMgr->getDependencyRegistry();
                // Can't directly emplace mgr into _services as that would result into modifying the container while iterating.
                _eventQueue->pushPrioritisedEvent<InsertServiceEvent>(serviceId, std::min(INTERNAL_INSERT_SERVICE_EVENT_PRIORITY, priority), std::move(cmpMgr));

                bool startImmediately = true;
                for (auto const &[key, registration] : reg->_registrations) {
                    auto const &props = std::get<tl::optional<Properties>>(registration);
                    if((std::get<Dependency>(registration).flags & DependencyFlags::REQUIRED) == DependencyFlags::REQUIRED) {
                        startImmediately = false;
                    }
                    _eventQueue->pushPrioritisedEvent<DependencyRequestEvent>(serviceId, event_priority, std::get<Dependency>(registration), props.has_value() ? &props.value() : tl::optional<Properties const *>{});
                }

                if(startImmediately) {
                    _eventQueue->pushPrioritisedEvent<StartServiceEvent>(serviceId, event_priority, serviceId);
                }

                if constexpr(IsConstructorInjector<Impl>) {
                    return ServiceProtectedPointer{static_cast<IService*>(impl)};
                } else {
                    return ServiceProtectedPointer{impl};
                }
            } else {
                static_assert(!(std::is_default_constructible_v<Impl> && RequestsProperties<Impl>), "Cannot have a properties constructor and a default constructor simultaneously.");
                auto cmpMgr = Detail::LifecycleManager<Impl, Interfaces...>::template create<>(std::forward<Properties>(properties), InterfacesList<Interfaces...>);
                auto serviceId = cmpMgr->serviceId();

                if constexpr (sizeof...(Interfaces) > 0) {
                    if constexpr (ListContainsInterface<IFrameworkLogger, Interfaces...>::value) {
                        static_assert(!IsConstructorInjector<Impl>, "Framework loggers cannot use constructor injection");
                        _logger = cmpMgr->getService().getImplementation();
                    }
                }

                cmpMgr->getService().setServicePriority(priority);

                logAddService<Impl, Interfaces...>(serviceId);

                if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                    if (_services.contains(serviceId)) [[unlikely]] {
                        ICHOR_EMERGENCY_LOG1(_logger, "Service id already exists. Bug in ichor?");
                        std::terminate();
                    }
                }

                Impl* impl = &cmpMgr->getService();
                _eventQueue->pushPrioritisedEvent<InsertServiceEvent>(serviceId, std::min(INTERNAL_INSERT_SERVICE_EVENT_PRIORITY, priority), std::move(cmpMgr));

                auto event_priority = std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, priority);
                _eventQueue->pushPrioritisedEvent<StartServiceEvent>(serviceId, event_priority, serviceId);

                if constexpr(IsConstructorInjector<Impl>) {
                    return ServiceProtectedPointer{static_cast<IService*>(impl)};
                } else {
                    return ServiceProtectedPointer{impl};
                }
            }
        }

        /// Convenience function to log adding of services
        /// \tparam Impl
        /// \tparam Interface1
        /// \tparam Interface2
        /// \tparam Interfaces
        /// \param id
        template <typename Impl, typename Interface1, typename Interface2, typename... Interfaces>
        void logAddService(ServiceIdType id) {
            if(_logger != nullptr && _logger->getLogLevel() <= LogLevel::LOG_DEBUG) {
                fmt::memory_buffer out;
                fmt::format_to(std::back_inserter(out), "added ServiceManager<{}, {}, ", typeName<Interface1>(), typeName<Interface2>());
                (fmt::format_to(std::back_inserter(out), "{}, ", typeName<Interfaces>()), ...);
                fmt::format_to(std::back_inserter(out), "{}>", typeName<Impl>());
                ICHOR_LOG_DEBUG(_logger, "{} {}", out.data(), id);
            }
        }

        /// Convenience function to log adding of services
        /// \tparam Impl
        /// \tparam Interface
        /// \param id
        template <typename Impl, typename Interface>
        void logAddService(ServiceIdType id) {
            if(_logger != nullptr && _logger->getLogLevel() <= LogLevel::LOG_DEBUG) {
                ICHOR_LOG_DEBUG(_logger, "added ServiceManager<{}, {}> {}", typeName<Interface>(), typeName<Impl>(), id);
            }
        }

        /// Convenience function to log adding of services
        /// \tparam Impl
        /// \param id
        template <typename Impl>
        void logAddService(ServiceIdType id) {
            if(_logger != nullptr && _logger->getLogLevel() <= LogLevel::LOG_DEBUG) {
                ICHOR_LOG_DEBUG(_logger, "added ServiceManager<{}> {}", typeName<Impl>(), id);
            }
        }

        void handleEventCompletion(Event const &evt);
        ///
        /// \param evt
        /// \return number of event handlers broadcasted to and a pointer to a potentially moved evt due to coroutines.
        [[nodiscard]] std::pair<uint64_t, Event*> broadcastEvent(std::unique_ptr<Event> &evt);
        /// Sets the communication channel. Only to be used from inside the CommunicationChannel class itself.
        /// \param channel
        void setCommunicationChannel(NeverNull<CommunicationChannel*> channel);
        /// Unlinks this DM from the communication channel. Only to be used from inside the CommunicationChannel class itself.
        void clearCommunicationChannel();
        /// Called from the queue implementation
        void start();
        /// Called from the queue implementation
        void processEvent(std::unique_ptr<Event> &evt);
        /// Called from the queue implementation
        void stop();
        /// Called from the queue implementation
        void addInternalServiceManager(std::unique_ptr<ILifecycleManager> svc);
        void clearServiceRegistrations(std::vector<EventInterceptInfo> &allEventInterceptorsCopy, std::vector<EventInterceptInfo> &eventInterceptorsCopy, ServiceIdType svcId);
        void removeInternalService(std::vector<EventInterceptInfo> &allEventInterceptorsCopy, std::vector<EventInterceptInfo> &eventInterceptorsCopy, ServiceIdType svcId);
        /// Check if there is a coroutine for the given serviceId that is still waiting on something
        /// \param serviceId
        /// \return
        [[nodiscard]] bool existingCoroutineFor(ServiceIdType serviceId) const noexcept;
        /// Coroutine based method to wait for a service to have finished with either DependencyOfflineEvent or StopServiceEvent
        /// \param serviceId
        /// \param eventType
        /// \return
        Task<void> waitForService(ServiceIdType serviceId, uint64_t eventType) noexcept;
        /// Counterpart for waitForService, runs all waiting coroutines for specified event
        /// \param serviceId
        /// \param eventType
        /// \param eventName
        /// \return
        bool finishWaitingService(ServiceIdType serviceId, uint64_t eventType, [[maybe_unused]] std::string_view eventName) noexcept;

        unordered_map<ServiceIdType, std::unique_ptr<ILifecycleManager>> _services{}; // key = service id
        unordered_map<DependencyTrackerKey, std::vector<DependencyTrackerInfo>, DependencyTrackerKeyHash, std::equal_to<>> _dependencyRequestTrackers{}; // key = interface name hash
        unordered_map<uint64_t, std::vector<EventCallbackInfo>> _eventCallbacks{}; // key = event id
        unordered_map<uint64_t, std::vector<EventInterceptInfo>> _eventInterceptors{}; // key = event id
        unordered_map<uint64_t, std::unique_ptr<IGenerator>> _scopedGenerators{}; // key = promise id
        unordered_map<uint64_t, ReferenceCountedPointer<Event>> _scopedEvents{}; // key = promise id
        unordered_map<uint64_t, EventWaiter> _eventWaiters{}; // key = event id
        unordered_map<ServiceIdType, EventWaiter> _dependencyWaiters{}; // key = service id
        IEventQueue *_eventQueue;
        IFrameworkLogger *_logger{};
        std::atomic<bool> _started{false};
        CommunicationChannel *_communicationChannel{};
        uint64_t _id{_managerIdCounter.fetch_add(1, std::memory_order_relaxed)};
        bool _quitEventReceived{};
        bool _quitDone{};
        static std::atomic<uint64_t> _managerIdCounter;

        friend class IEventQueue;
        friend class ILifecycleManager;
        friend class CommunicationChannel;
    };


    namespace Detail {
        // Used internally to insert events where passing around the DM isn't feasible
        // Only use if you know for sure you're on the correct thread.
        constinit thread_local extern DependencyManager *_local_dm;
    }

    /// Returns thread-local manager. Can only be used after the manager's start() function has been called.
    /// \return
    [[nodiscard]] DependencyManager& GetThreadLocalManager() noexcept;
}

#include <ichor/coroutines/AsyncGeneratorDetail.h>
