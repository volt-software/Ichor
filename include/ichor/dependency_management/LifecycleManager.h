#pragma once

namespace Ichor::Detail {
    extern thread_local unordered_set<uint64_t> emptyDependencies;

    /// This lifecycle manager is created when the underlying service requests 0 dependencies
    /// It contains optimizations for dealing with dependencies.
    /// \tparam ServiceType
    /// \tparam IFaces
    template<class ServiceType, typename... IFaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
    requires DerivedTemplated<ServiceType, AdvancedService> || IsConstructorInjector<ServiceType>
#endif
    class LifecycleManager final : public ILifecycleManager {
    public:
        template <typename U = ServiceType> requires RequestsProperties<U>
        explicit LifecycleManager(Properties&& properties) : _interfaces(), _service(std::forward<Properties>(properties)) {
            (_interfaces.emplace_back(typeNameHash<IFaces>(), typeName<IFaces>(), DependencyFlags::NONE, false),...);
        }

        template <typename U = ServiceType> requires (!RequestsProperties<U>)
        explicit LifecycleManager(Properties&& properties) : _interfaces(), _service() {
            (_interfaces.emplace_back(typeNameHash<IFaces>(), typeName<IFaces>(), DependencyFlags::NONE, false),...);
            _service.setProperties(std::forward<Properties>(properties));
        }

        ~LifecycleManager() final = default;

        template<typename... Interfaces>
        [[nodiscard]]
        static std::unique_ptr<LifecycleManager<ServiceType, Interfaces...>> create(Properties&& properties, InterfacesList_t<Interfaces...>) {
            return std::make_unique<LifecycleManager<ServiceType, Interfaces...>>(std::forward<Properties>(properties));
        }

        std::vector<Dependency*> interestedInDependencyGoingOffline(ILifecycleManager *dependentService) noexcept final {
            return {};
        }

        StartBehaviour dependencyOnline(NeverNull<ILifecycleManager*> dependentService) final {
            return StartBehaviour::DONE;
        }

        AsyncGenerator<StartBehaviour> dependencyOffline(NeverNull<ILifecycleManager*> dependentService, std::vector<Dependency*> deps) final {
            // this function should never be called
            std::terminate();
        }

        [[nodiscard]]
        unordered_set<uint64_t> &getDependencies() noexcept final {
            return emptyDependencies;
        }

        [[nodiscard]]
        unordered_set<uint64_t> &getDependees() noexcept final {
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
            co_return co_await _service.internal_start(nullptr);
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

        [[nodiscard]] uint64_t serviceId() const noexcept final {
            return _service.getServiceId();
        }

        [[nodiscard]] ServiceType& getService() noexcept {
            return _service;
        }

        [[nodiscard]] uint64_t getPriority() const noexcept final {
            return _service.getServicePriority();
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

        [[nodiscard]] std::span<Dependency const> getInterfaces() const noexcept final {
            return std::span<Dependency const>{_interfaces.begin(), _interfaces.size()};
        }

        [[nodiscard]] Properties const & getProperties() const noexcept final {
            return _service._properties;
        }

        [[nodiscard]] DependencyRegister const * getDependencyRegistry() const noexcept final {
            return nullptr;
        }

        /// Someone is interested in us, inject ourself into them
        /// \param keyOfInterfaceToInject
        /// \param serviceIdOfOther
        /// \param fn
        void insertSelfInto(uint64_t keyOfInterfaceToInject, uint64_t serviceIdOfOther, std::function<void(NeverNull<void*>, IService&)> &fn) final {
            if constexpr (sizeof...(IFaces) > 0) {
                insertSelfInto2<sizeof...(IFaces), IFaces...>(keyOfInterfaceToInject, fn);
                _serviceIdsOfDependees.insert(serviceIdOfOther);
            }
        }

        /// See insertSelfInto
        /// \param keyOfInterfaceToInject
        /// \param serviceIdOfOther
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

        /// The underlying service got stopped and someone else is asking us to remove ourselves from them.
        /// \param keyOfInterfaceToInject
        /// \param serviceIdOfOther
        /// \param fn
        void removeSelfInto(uint64_t keyOfInterfaceToInject, uint64_t serviceIdOfOther, std::function<void(NeverNull<void*>, IService&)> &fn) final {
            INTERNAL_DEBUG("removeSelfInto2() svc {} removing svc {}", serviceId(), serviceIdOfOther);
            if constexpr (sizeof...(IFaces) > 0) {
                insertSelfInto2<sizeof...(IFaces), IFaces...>(keyOfInterfaceToInject, fn);
                // This is erased by the dependency manager
                _serviceIdsOfDependees.erase(serviceIdOfOther);
            }
        }

    private:
        StaticVector<Dependency, sizeof...(IFaces)> _interfaces;
        ServiceType _service;
        unordered_set<uint64_t> _serviceIdsOfDependees; // services that depend on this service
    };
}
