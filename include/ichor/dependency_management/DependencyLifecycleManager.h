#pragma once

#include <ichor/event_queues/IEventQueue.h>

namespace Ichor::Detail {

    template<class ServiceType, typename... IFaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
    requires Derived<ServiceType, IService>
#endif
    class DependencyLifecycleManager final : public ILifecycleManager {
    public:
        explicit DependencyLifecycleManager(std::vector<Dependency> interfaces, Properties&& properties) : _interfaces(std::move(interfaces)), _registry(), _dependencies(), _service(_registry, std::move(properties)) {
            for(auto const &reg : _registry._registrations) {
                _dependencies.addDependency(std::get<0>(reg.second));
            }
        }

        ~DependencyLifecycleManager() final {
            INTERNAL_DEBUG("destroying {}, id {}", typeName<ServiceType>(), _service.getServiceId());
            for(auto const &dep : _dependencies._dependencies) {
                GetThreadLocalEventQueue().template pushPrioritisedEvent<DependencyUndoRequestEvent>(_service.getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, dep, getProperties());
            }
        }

        template<typename... Interfaces>
        [[nodiscard]]
        static std::unique_ptr<DependencyLifecycleManager<ServiceType, Interfaces...>> create(Properties&& properties, InterfacesList_t<Interfaces...>) {
            std::vector<Dependency> interfaces{};
            interfaces.reserve(sizeof...(Interfaces));
            (interfaces.emplace_back(typeNameHash<Interfaces>(), typeName<Interfaces>(), DependencyFlags::NONE, false),...);
            return std::make_unique<DependencyLifecycleManager<ServiceType, Interfaces...>>(std::move(interfaces), std::move(properties));
        }

        std::vector<Dependency*> interestedInDependencyGoingOffline(ILifecycleManager *dependentService) noexcept final {
            std::vector<Dependency*> ret;

            if(!_serviceIdsOfInjectedDependencies.contains(dependentService->serviceId())) {
                fmt::print("interestedInDependencyGoingOffline() svc {}:{} already injected\n", serviceId(), implementationName());
                return ret;
            }

            for(auto const &interface : dependentService->getInterfaces()) {
                auto dep = _dependencies.find(interface, true);
                INTERNAL_DEBUG("interestedInDependencyGoingOffline() svc {}:{} {} dependent {}:{}", serviceId(), implementationName(), getServiceState(), dependentService->serviceId(), dependentService->implementationName());

                if(dep == _dependencies.end()) {
                    INTERNAL_DEBUG("interestedInDependencyGoingOffline() not found");
                    continue;
                }

                ret.push_back(&(*dep));
            }

            return ret;
        }

        StartBehaviour dependencyOnline(NeverNull<ILifecycleManager*> dependentService) final {
            INTERNAL_DEBUG("dependencyOnline() svc {}:{} {} dependent {}:{}", serviceId(), implementationName(), getServiceState(), dependentService->serviceId(), dependentService->implementationName());

            if(_serviceIdsOfInjectedDependencies.contains(dependentService->serviceId())) {
                fmt::print("dependencyOnline() svc {}:{} already injected\n", serviceId(), implementationName());
                return StartBehaviour::DONE;
            }

            if constexpr (DO_INTERNAL_DEBUG) {
                for (auto id: _serviceIdsOfInjectedDependencies) {
                    INTERNAL_DEBUG("dependency: {}", id);
                }
                for (auto id: _serviceIdsOfDependees) {
                    INTERNAL_DEBUG("dependee: {}", id);
                }
            }

            auto interested = DependencyChange::NOT_FOUND;

            for(auto const &interface : dependentService->getInterfaces()) {
                auto dep = _dependencies.find(interface, false);

                if(dep == _dependencies.end()) {
                    continue;
                }

                INTERNAL_DEBUG("dependencyOnline() dep {} {} {}", dep->interfaceName, dep->satisfied, dep->flags);
                if(dep->satisfied == 0) {
                    interested = DependencyChange::FOUND;
                }

                _serviceIdsOfInjectedDependencies.insert(dependentService->serviceId());
                injectIntoSelfDoubleDispatch(dep->interfaceNameHash, dependentService);
                dep->satisfied++;
            }

            if(interested == DependencyChange::FOUND && getServiceState() <= ServiceState::INSTALLED && _dependencies.allSatisfied()) {
                return StartBehaviour::STARTED;
            }

            return StartBehaviour::DONE;
        }

        AsyncGenerator<StartBehaviour> dependencyOffline(NeverNull<ILifecycleManager*> dependentService, std::vector<Dependency*> deps) final {
            INTERNAL_DEBUG("dependencyOffline() svc {}:{} {} dependent {}:{}", serviceId(), implementationName(), getServiceState(), dependentService->serviceId(), dependentService->implementationName());

            if(!_serviceIdsOfInjectedDependencies.contains(dependentService->serviceId())) {
                fmt::print("dependencyOffline() svc {}:{} not injected\n", serviceId(), implementationName());
                co_return StartBehaviour::DONE;
            }

            auto interested = DependencyChange::NOT_FOUND;
            StartBehaviour ret = StartBehaviour::DONE;

            if constexpr (DO_INTERNAL_DEBUG) {
                for (auto id: _serviceIdsOfInjectedDependencies) {
                    INTERNAL_DEBUG("dependency: {}", id);
                }
                for (auto id: _serviceIdsOfDependees) {
                    INTERNAL_DEBUG("dependee: {}", id);
                }
            }

            for(auto *dep : deps) {
                // dependency should not be marked as unsatisfied if there is at least one other of the same type present
                dep->satisfied--;
                _serviceIdsOfInjectedDependencies.erase(dependentService->serviceId());

#ifdef ICHOR_USE_HARDENING
                if(dep->satisfied == std::numeric_limits<decltype(dep->satisfied)>::max()) [[unlikely]] {
                    std::terminate();
                }
#endif

                if((dep->flags & DependencyFlags::REQUIRED) && dep->satisfied == 0 && (getServiceState() == ServiceState::STARTING || getServiceState() == ServiceState::INJECTING)) {
                    INTERNAL_DEBUG("{}:{}:{} dependencyOffline waitForService {} {} {} {}", serviceId(), _service.getServiceName(), getServiceState(), interested, dep->satisfied, dep->flags, getDependees().size());
                    co_await waitForService(serviceId(), DependencyOnlineEvent::TYPE);
                }

                if ((dep->flags & DependencyFlags::REQUIRED) && dep->satisfied == 0 && interested != DependencyChange::FOUND_AND_STOP_ME && getServiceState() == ServiceState::ACTIVE) {
                    INTERNAL_DEBUG("{}:{}:{} dependencyOffline stopping {}", serviceId(), _service.getServiceName(), getServiceState(), interested);

                    GetThreadLocalEventQueue().template pushPrioritisedEvent<DependencyOfflineEvent>(serviceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY - 1);
                    co_await waitForService(serviceId(), DependencyOfflineEvent::TYPE);
                    INTERNAL_DEBUG("{}:{}:{} dependencyOffline stopping {} pushPrioritisedEventAsync done", serviceId(), _service.getServiceName(), getServiceState(), interested);

#ifdef ICHOR_USE_HARDENING
                    if(!_serviceIdsOfDependees.empty()) [[unlikely]] {
                        for(auto d : _serviceIdsOfDependees) {
                            fmt::print("{}\n", d);
                        }
                        std::terminate();
                    }
#endif

                    if(getServiceState() == ServiceState::UNINJECTING) {
                        interested = DependencyChange::FOUND_AND_STOP_ME;
                        auto gen = stop();
                        auto it = gen.begin();
                        co_await it;

#ifdef ICHOR_USE_HARDENING
                        if (!it.get_finished()) [[unlikely]] {
                            std::terminate();
                        }
                        if (getServiceState() != ServiceState::INSTALLED) [[unlikely]] {
                            std::terminate();
                        }
#endif

                        ret = StartBehaviour::STOPPED;
                        INTERNAL_DEBUG("{}:{}:{} dependencyOffline stopped {}", serviceId(), _service.getServiceName(), getServiceState(), interested);
                    } else {
                        interested = DependencyChange::FOUND;
                    }
                } else if(interested != DependencyChange::FOUND_AND_STOP_ME) {
                    interested = DependencyChange::FOUND;
                }

                if((dep->flags & DependencyFlags::REQUIRED) && dep->satisfied == 0 && (getServiceState() == ServiceState::UNINJECTING || getServiceState() == ServiceState::STOPPING)) {
                    INTERNAL_DEBUG("{}:{}:{} dependencyOffline waitForService {} {} {} {}", serviceId(), _service.getServiceName(), getServiceState(), interested, dep->satisfied, dep->flags, getDependees().size());
                    co_await waitForService(serviceId(), StopServiceEvent::TYPE);
                }

#ifdef ICHOR_USE_HARDENING
                if((dep->flags & DependencyFlags::REQUIRED) && dep->satisfied == 0 && getServiceState() >= ServiceState::INJECTING) [[unlikely]] {
                    INTERNAL_DEBUG("{}:{}:{} dependencyOffline terminating {} {} {} {}", serviceId(), _service.getServiceName(), getServiceState(), interested, dep->satisfied, dep->flags, getDependees().size());
                    std::terminate();
                }
#endif

                INTERNAL_DEBUG("{}:{}:{} dependencyOffline interested {} {} {}", serviceId(), _service.getServiceName(), getServiceState(), interested, dep->satisfied, dep->flags);
                removeSelfIntoDoubleDispatch(dep->interfaceNameHash, dependentService);
            }

            co_return ret;
        }

        /// We found a dependency that we're interested in, tell that dependency to inject itself into us
        /// We take this roundabout way because we have no way of casting the servicetype embedded in the lifecycle manager to the correct type
        /// \param keyOfInterfaceToInject
        /// \param dependentService
        void injectIntoSelfDoubleDispatch(uint64_t keyOfInterfaceToInject, NeverNull<ILifecycleManager*> dependentService) {
            INTERNAL_DEBUG("injectIntoSelfDoubleDispatch() svc {} adding dependency {}", serviceId(), dependentService->serviceId());
            auto dep = _registry._registrations.find(keyOfInterfaceToInject);

#ifdef ICHOR_USE_HARDENING
            if(dep == end(_registry._registrations)) [[unlikely]] {
                std::terminate();
            }
#endif

            dependentService->insertSelfInto(keyOfInterfaceToInject, serviceId(), std::get<1>(dep->second));
        }

        /// Someone is interested in us, inject ourself into them
        /// \param keyOfInterfaceToInject
        /// \param serviceIdOfOther
        /// \param fn
        void insertSelfInto(uint64_t keyOfInterfaceToInject, ServiceIdType serviceIdOfOther, std::function<void(NeverNull<void*>, IService&)> &fn) final {
            INTERNAL_DEBUG("insertSelfInto() svc {} telling svc {} to add us", serviceId(), serviceIdOfOther);
            if constexpr (sizeof...(IFaces) > 0) {
                insertSelfInto2<sizeof...(IFaces), IFaces...>(keyOfInterfaceToInject, fn);
                _serviceIdsOfDependees.insert(serviceIdOfOther);
            }
        }

        /// Someone is interested in us, inject ourself into them. Recursive templated function
        /// \tparam i
        /// \tparam Iface1
        /// \tparam otherIfaces
        /// \param keyOfInterfaceToInject
        /// \param fn
        template <int i, typename Iface1, typename... otherIfaces>
        void insertSelfInto2(uint64_t keyOfInterfaceToInject, std::function<void(NeverNull<void*>, IService&)> &fn) {
            if(typeNameHash<Iface1>() == keyOfInterfaceToInject) {
                fn(static_cast<Iface1*>(_service.getImplementation()), static_cast<IService&>(_service));
            } else {
                if constexpr(i > 1) {
                    insertSelfInto2<sizeof...(otherIfaces), otherIfaces...>(keyOfInterfaceToInject, fn);
                }
            }
        }

        void removeSelfIntoDoubleDispatch(uint64_t keyOfInterfaceToInject, NeverNull<ILifecycleManager*> dependentService) {
            INTERNAL_DEBUG("removeSelfIntoDoubleDispatch() svc {} removing dependency {}", serviceId(), dependentService->serviceId());
            auto dep = _registry._registrations.find(keyOfInterfaceToInject);

#ifdef ICHOR_USE_HARDENING
            if(dep == end(_registry._registrations)) [[unlikely]] {
                std::terminate();
            }
#endif

            dependentService->removeSelfInto(keyOfInterfaceToInject, serviceId(), std::get<2>(dep->second));
        }

        /// The underlying service got stopped and someone else is asking us to remove ourselves from them.
        /// \param keyOfInterfaceToInject
        /// \param serviceIdOfOther
        /// \param fn
        void removeSelfInto(uint64_t keyOfInterfaceToInject, ServiceIdType serviceIdOfOther, std::function<void(NeverNull<void*>, IService&)> &fn) final {
            INTERNAL_DEBUG("removeSelfInto() svc {} telling svc {} to remove us", serviceId(), serviceIdOfOther);
            if constexpr (sizeof...(IFaces) > 0) {
                insertSelfInto2<sizeof...(IFaces), IFaces...>(keyOfInterfaceToInject, fn);
                // This is erased by the dependency manager
                _serviceIdsOfDependees.erase(serviceIdOfOther);
            }
        }

        [[nodiscard]]
        unordered_set<ServiceIdType> &getDependencies() noexcept final {
            return _serviceIdsOfInjectedDependencies;
        }

        [[nodiscard]]
        unordered_set<ServiceIdType> &getDependees() noexcept final {
            return _serviceIdsOfDependees;
        }

        [[nodiscard]]
        AsyncGenerator<StartBehaviour> startAfterDependencyOnline() final {
            auto startBehaviour = co_await _service.internal_start(nullptr);

            if(startBehaviour == StartBehaviour::DONE) {
                co_return StartBehaviour::STARTED;
            }

            co_return startBehaviour;
        }

        [[nodiscard]]
        AsyncGenerator<StartBehaviour> start() final {
            co_return co_await _service.internal_start(&_dependencies);
        }

        [[nodiscard]]
        AsyncGenerator<StartBehaviour> stop() final {
            co_return co_await _service.internal_stop();
        }

        [[nodiscard]]
        bool setInjected() final {
            return _service.internalSetInjected();
        }

        [[nodiscard]]
        bool setUninjected() final {
            return _service.internalSetUninjected();
        }

        [[nodiscard]] std::string_view implementationName() const noexcept final {
            return _service.getServiceName();
        }

        [[nodiscard]] uint64_t type() const noexcept final {
            return typeNameHash<ServiceType>();
        }

        [[nodiscard]] ServiceIdType serviceId() const noexcept final {
            return _service.getServiceId();
        }

        [[nodiscard]] uint64_t getPriority() const noexcept final {
            return _service.getServicePriority();
        }

        [[nodiscard]] ServiceType& getService() noexcept {
            return _service;
        }

        [[nodiscard]] ServiceState getServiceState() const noexcept final {
            return _service.getState();
        }

        [[nodiscard]] NeverNull<IService*> getIService() noexcept final {
            return static_cast<IService *>(&_service);
        }

        [[nodiscard]] NeverNull<IService const*> getIService() const noexcept final {
            return static_cast<IService const *>(&_service);
        }

        [[nodiscard]] const std::vector<Dependency>& getInterfaces() const noexcept final {
            return _interfaces;
        }

        [[nodiscard]] Properties const & getProperties() const noexcept final {
            return _service._properties;
        }

        [[nodiscard]] DependencyRegister const * getDependencyRegistry() const noexcept final {
            return &_registry;
        }

    private:
        std::vector<Dependency> _interfaces;
        DependencyRegister _registry;
        DependencyInfo _dependencies;
        ServiceType _service;
        unordered_set<ServiceIdType> _serviceIdsOfInjectedDependencies; // Services that this service depends on.
        unordered_set<ServiceIdType> _serviceIdsOfDependees; // services that depend on this service
    };
}
