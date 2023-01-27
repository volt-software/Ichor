#pragma once

namespace Ichor {

    template<class ServiceType, typename... IFaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
    requires Derived<ServiceType, IService>
#endif
    class DependencyLifecycleManager final : public ILifecycleManager {
    public:
        explicit DependencyLifecycleManager(std::vector<Dependency> interfaces, Properties&& properties, DependencyManager *mng) : _interfaces(std::move(interfaces)), _registry(), _dependencies(), _service(_registry, std::move(properties), mng) {
            for(auto const &reg : _registry._registrations) {
                _dependencies.addDependency(std::get<0>(reg.second));
            }
        }

        ~DependencyLifecycleManager() final {
            INTERNAL_DEBUG("destroying {}, id {}", typeName<ServiceType>(), _service.getServiceId());
            for(auto const &dep : _dependencies._dependencies) {
                // _manager is always injected in DependencyManager::create...Manager functions.
                _service._manager->template pushPrioritisedEvent<DependencyUndoRequestEvent>(_service.getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, Dependency{dep.interfaceNameHash, dep.required, dep.satisfied}, getProperties());
            }
        }

        template<typename... Interfaces>
        [[nodiscard]]
        static std::unique_ptr<DependencyLifecycleManager<ServiceType, Interfaces...>> create(Properties&& properties, DependencyManager *mng, InterfacesList_t<Interfaces...>) {
            std::vector<Dependency> interfaces{};
            interfaces.reserve(sizeof...(Interfaces));
            (interfaces.emplace_back(typeNameHash<Interfaces>(), false, false),...);
            return std::make_unique<DependencyLifecycleManager<ServiceType, Interfaces...>>(std::move(interfaces), std::move(properties), mng);
        }

        std::vector<decltype(std::declval<DependencyInfo>().begin())> interestedInDependency(ILifecycleManager *dependentService, bool online) noexcept final {
            if((online && _serviceIdsOfInjectedDependencies.contains(dependentService->serviceId())) || (!online && !_serviceIdsOfInjectedDependencies.contains(dependentService->serviceId()))) {
                return {};
            }

            std::vector<decltype(std::declval<DependencyInfo>().begin())> ret;

            for(auto const &interface : dependentService->getInterfaces()) {
                auto dep = _dependencies.find(interface);

                if (dep == _dependencies.end()) {
                    continue;
                }

                ret.push_back(dep);
            }

            return ret;
        }

        AsyncGenerator<StartBehaviour> dependencyOnline(ILifecycleManager* dependentService, std::vector<decltype(std::declval<DependencyInfo>().begin())> iterators) final {
            INTERNAL_DEBUG("dependencyOnline() svc {}:{} {} dependent {}:{}", serviceId(), implementationName(), getServiceState(), dependentService->serviceId(), dependentService->implementationName());
            if constexpr (DO_INTERNAL_DEBUG) {
                for (auto id: _serviceIdsOfInjectedDependencies) {
                    INTERNAL_DEBUG("dependency: {}", id);
                }
                for (auto id: _serviceIdsOfDependees) {
                    INTERNAL_DEBUG("dependee: {}", id);
                }
            }

            auto interested = Detail::DependencyChange::NOT_FOUND;

            for(auto &dep : iterators) {
                if(dep->satisfied == 0) {
                    interested = Detail::DependencyChange::FOUND;
                }

                _serviceIdsOfInjectedDependencies.insert(dependentService->serviceId());
                injectIntoSelfDoubleDispatch(dep->interfaceNameHash, dependentService);
                dep->satisfied++;
            }

            if(interested == Detail::DependencyChange::FOUND && getServiceState() <= ServiceState::INSTALLED && _dependencies.allSatisfied()) {
                auto gen = _service.internal_start(nullptr); // we already checked the dependencies, pass in nullptr
                auto it = gen.begin();
                StartBehaviour ret = *co_await it;

#ifdef ICHOR_USE_HARDENING
                if(!it.get_finished()) [[unlikely]] {
                    std::terminate();
                }
#endif

                if(ret == StartBehaviour::STOPPED) {
                    co_return StartBehaviour::STOPPED;
                }

#ifdef ICHOR_USE_HARDENING
                if(getServiceState() != ServiceState::INJECTING) [[unlikely]] {
                    std::terminate();
                }
#endif
                co_return StartBehaviour::STARTED;
            }

            co_return StartBehaviour::DONE;
        }

        AsyncGenerator<StartBehaviour> dependencyOffline(ILifecycleManager* dependentService, std::vector<decltype(std::declval<DependencyInfo>().begin())> iterators) final {
            INTERNAL_DEBUG("dependencyOffline() svc {}:{} {} dependent {}:{}", serviceId(), implementationName(), getServiceState(), dependentService->serviceId(), dependentService->implementationName());
            auto interested = Detail::DependencyChange::NOT_FOUND;
            StartBehaviour ret = StartBehaviour::DONE;

            if constexpr (DO_INTERNAL_DEBUG) {
                for (auto id: _serviceIdsOfInjectedDependencies) {
                    INTERNAL_DEBUG("dependency: {}", id);
                }
                for (auto id: _serviceIdsOfDependees) {
                    INTERNAL_DEBUG("dependee: {}", id);
                }
            }

            for(auto &dep : iterators) {
                // dependency should not be marked as unsatisfied if there is at least one other of the same type present
                dep->satisfied--;
                _serviceIdsOfInjectedDependencies.erase(dependentService->serviceId());

#ifdef ICHOR_USE_HARDENING
                if(dep->satisfied == std::numeric_limits<decltype(dep->satisfied)>::max()) [[unlikely]] {
                    std::terminate();
                }
#endif

                if(dep->required && dep->satisfied == 0 && (getServiceState() == ServiceState::STARTING || getServiceState() == ServiceState::INJECTING)) {
                    INTERNAL_DEBUG("{}:{}:{} dependencyOffline waitForService {} {} {} {}", serviceId(), _service.getServiceName(), getServiceState(), interested, dep->satisfied, dep->required, getDependees().size());
                    co_await waitForService(_service.getManager(), serviceId(), DependencyOnlineEvent::TYPE).begin();
                }

                if (dep->required && dep->satisfied == 0 && interested != Detail::DependencyChange::FOUND_AND_STOP_ME && getServiceState() == ServiceState::ACTIVE) {
                    INTERNAL_DEBUG("{}:{}:{} dependencyOffline stopping {}", serviceId(), _service.getServiceName(), getServiceState(), interested);

                    _service._manager->template pushPrioritisedEvent<DependencyOfflineEvent>(serviceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY - 1);
                    co_await waitForService(_service.getManager(), serviceId(), DependencyOfflineEvent::TYPE).begin();
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
                        interested = Detail::DependencyChange::FOUND_AND_STOP_ME;
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
                        interested = Detail::DependencyChange::FOUND;
                    }
                } else if(interested != Detail::DependencyChange::FOUND_AND_STOP_ME) {
                    interested = Detail::DependencyChange::FOUND;
                }

                if(dep->required && dep->satisfied == 0 && (getServiceState() == ServiceState::UNINJECTING || getServiceState() == ServiceState::STOPPING)) {
                    INTERNAL_DEBUG("{}:{}:{} dependencyOffline waitForService {} {} {} {}", serviceId(), _service.getServiceName(), getServiceState(), interested, dep->satisfied, dep->required, getDependees().size());
                    co_await waitForService(_service.getManager(), serviceId(), StopServiceEvent::TYPE).begin();
                }

#ifdef ICHOR_USE_HARDENING
                if(dep->required && dep->satisfied == 0 && getServiceState() >= ServiceState::INJECTING) [[unlikely]] {
                    INTERNAL_DEBUG("{}:{}:{} dependencyOffline terminating {} {} {} {}", serviceId(), _service.getServiceName(), getServiceState(), interested, dep->satisfied, dep->required, getDependees().size());
                    std::terminate();
                }
#endif

                INTERNAL_DEBUG("{}:{}:{} dependencyOffline interested {} {} {}", serviceId(), _service.getServiceName(), getServiceState(), interested, dep->satisfied, dep->required);
                removeSelfIntoDoubleDispatch(dep->interfaceNameHash, dependentService);
            }

            co_return ret;
        }

        /// We found a dependency that we're interested in, tell that dependency to inject itself into us
        /// We take this roundabout way because we have no way of casting the servicetype embedded in the lifecycle manager to the correct type
        /// \param keyOfInterfaceToInject
        /// \param dependentService
        void injectIntoSelfDoubleDispatch(uint64_t keyOfInterfaceToInject, ILifecycleManager* dependentService) {
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
        void insertSelfInto(uint64_t keyOfInterfaceToInject, uint64_t serviceIdOfOther, std::function<void(void*, IService*)> &fn) final {
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
        void insertSelfInto2(uint64_t keyOfInterfaceToInject, std::function<void(void*, IService*)> &fn) {
            if(typeNameHash<Iface1>() == keyOfInterfaceToInject) {
                fn(static_cast<Iface1*>(&_service), static_cast<IService*>(&_service));
            } else {
                if constexpr(i > 1) {
                    insertSelfInto2<sizeof...(otherIfaces), otherIfaces...>(keyOfInterfaceToInject, fn);
                }
            }
        }

        void removeSelfIntoDoubleDispatch(uint64_t keyOfInterfaceToInject, ILifecycleManager* dependentService) {
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
        void removeSelfInto(uint64_t keyOfInterfaceToInject, uint64_t serviceIdOfOther, std::function<void(void*, IService*)> &fn) final {
            INTERNAL_DEBUG("removeSelfInto() svc {} telling svc {} to remove us", serviceId(), serviceIdOfOther);
            if constexpr (sizeof...(IFaces) > 0) {
                insertSelfInto2<sizeof...(IFaces), IFaces...>(keyOfInterfaceToInject, fn);
                // This is erased by the dependency manager
                _serviceIdsOfDependees.erase(serviceIdOfOther);
            }
        }

        [[nodiscard]]
        unordered_set<uint64_t> &getDependencies() noexcept final {
            return _serviceIdsOfInjectedDependencies;
        }

        [[nodiscard]]
        unordered_set<uint64_t> &getDependees() noexcept final {
            return _serviceIdsOfDependees;
        }

        [[nodiscard]]
        AsyncGenerator<StartBehaviour> start() final {
            return _service.internal_start(&_dependencies);
        }

        [[nodiscard]]
        AsyncGenerator<StartBehaviour> stop() final {
            return _service.internal_stop();
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

        [[nodiscard]] uint64_t serviceId() const noexcept final {
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

        [[nodiscard]] IService const * getIService() const noexcept final {
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
        unordered_set<uint64_t> _serviceIdsOfInjectedDependencies; // Services that this service depends on.
        unordered_set<uint64_t> _serviceIdsOfDependees; // services that depend on this service
    };
}
