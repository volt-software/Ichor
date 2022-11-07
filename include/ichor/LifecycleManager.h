#pragma once

#include <string_view>
#include <memory>
#include <ichor/Service.h>
#include <ichor/Common.h>
#include <ichor/events/Event.h>
#include <ichor/Dependency.h>
#include <ichor/DependencyInfo.h>
#include <ichor/DependencyRegister.h>

namespace Ichor {
    namespace Detail {
        extern unordered_set<uint64_t> emptyDependencies;
    }

    class ILifecycleManager {
    public:
        virtual ~ILifecycleManager() = default;
        ///
        /// \param dependentService
        /// \return true if dependency is registered in service, false if not
        virtual Detail::DependencyChange dependencyOnline(ILifecycleManager* dependentService) = 0;
        ///
        /// \param dependentService
        /// \return true if dependency is registered in service, false if not
        virtual Detail::DependencyChange dependencyOffline(ILifecycleManager* dependentService) = 0;
        [[nodiscard]] virtual unordered_set<uint64_t> &getDependencies() noexcept = 0;
        [[nodiscard]] virtual unordered_set<uint64_t> &getDependees() noexcept = 0;
        [[nodiscard]] virtual StartBehaviour start() = 0;
        [[nodiscard]] virtual StartBehaviour stop() = 0;
        [[nodiscard]] virtual bool setInjected() = 0;
        [[nodiscard]] virtual bool setUninjected() = 0;
        [[nodiscard]] virtual std::string_view implementationName() const noexcept = 0;
        [[nodiscard]] virtual uint64_t type() const noexcept = 0;
        [[nodiscard]] virtual uint64_t serviceId() const noexcept = 0;
        [[nodiscard]] virtual uint64_t getPriority() const noexcept = 0;
        [[nodiscard]] virtual ServiceState getServiceState() const noexcept = 0;
        [[nodiscard]] virtual const std::vector<Dependency>& getInterfaces() const noexcept = 0;
        [[nodiscard]] virtual Properties const & getProperties() const noexcept = 0;
        [[nodiscard]] virtual DependencyRegister const * getDependencyRegistry() const noexcept = 0;
        virtual void insertSelfInto(uint64_t keyOfInterfaceToInject, uint64_t serviceIdOfOther, std::function<void(void*, IService*)>&) = 0;
        virtual void removeSelfInto(uint64_t keyOfInterfaceToInject, uint64_t serviceIdOfOther, std::function<void(void*, IService*)>&) = 0;
    };

    template<class ServiceType, typename... IFaces>
    requires DerivedTemplated<ServiceType, Service>
    class DependencyLifecycleManager final : public ILifecycleManager {
    public:
        explicit DependencyLifecycleManager(std::vector<Dependency> interfaces, Properties&& properties, DependencyManager *mng) : _implementationName(typeName<ServiceType>()), _interfaces(std::move(interfaces)), _registry(mng), _dependencies(), _service(_registry, std::forward<Properties>(properties), mng) {
            for(auto const &reg : _registry._registrations) {
                _dependencies.addDependency(std::get<0>(reg.second));
            }
        }

        ~DependencyLifecycleManager() final {
            INTERNAL_DEBUG("destroying {}, id {}", typeName<ServiceType>(), _service.getServiceId());
            for(auto const &dep : _dependencies._dependencies) {
                // _manager is always injected in DependencyManager::create...Manager functions.
                _service._manager->template pushPrioritisedEvent<DependencyUndoRequestEvent>(_service.getServiceId(), getPriority(), Dependency{dep.interfaceNameHash, dep.required, dep.satisfied}, &getProperties());
            }
        }

        template<typename... Interfaces>
        [[nodiscard]]
        static std::unique_ptr<DependencyLifecycleManager<ServiceType, Interfaces...>> create(Properties&& properties, DependencyManager *mng, InterfacesList_t<Interfaces...>) {
            std::vector<Dependency> interfaces{};
            interfaces.reserve(sizeof...(Interfaces));
            (interfaces.emplace_back(typeNameHash<Interfaces>(), false, false),...);
            return std::make_unique<DependencyLifecycleManager<ServiceType, Interfaces...>>(std::move(interfaces), std::forward<Properties>(properties), mng);
        }

        Detail::DependencyChange dependencyOnline(ILifecycleManager* dependentService) final {
            auto interested = Detail::DependencyChange::NOT_FOUND;
            auto const &interfaces = dependentService->getInterfaces();

            for(auto const &interface : interfaces) {
                auto dep = _dependencies.find(interface);
                if (dep == _dependencies.end()) {
                    continue;
                }

                if(dep->satisfied == 0) {
                    interested = Detail::DependencyChange::FOUND;
                }

                injectIntoSelfDoubleDispatch(interface.interfaceNameHash, dependentService);
                dep->satisfied++;
            }

            if(interested == Detail::DependencyChange::FOUND && _dependencies.allSatisfied()) {
                interested = Detail::DependencyChange::FOUND_AND_START_ME;
            }

            return interested;
        }

        void injectIntoSelfDoubleDispatch(uint64_t keyOfInterfaceToInject, ILifecycleManager* dependentService) {
            INTERNAL_DEBUG("injectIntoSelfDoubleDispatch() svc {} adding svc {}", serviceId(), dependentService->serviceId());
            auto dep = _registry._registrations.find(keyOfInterfaceToInject);

            if(dep != end(_registry._registrations)) {
                dependentService->insertSelfInto(keyOfInterfaceToInject, serviceId(), std::get<1>(dep->second));
                _serviceIdsOfInjectedDependencies.insert(dependentService->serviceId());
            }
        }

        void insertSelfInto(uint64_t keyOfInterfaceToInject, uint64_t serviceIdOfOther, std::function<void(void*, IService*)> &fn) final {
            INTERNAL_DEBUG("insertSelfInto() svc {} adding svc {}", serviceId(), serviceIdOfOther);
            if constexpr (sizeof...(IFaces) > 0) {
                insertSelfInto2<sizeof...(IFaces), IFaces...>(keyOfInterfaceToInject, fn);
                _serviceIdsOfDependees.insert(serviceIdOfOther);
            }
        }

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

        void removeSelfInto(uint64_t keyOfInterfaceToInject, uint64_t serviceIdOfOther, std::function<void(void*, IService*)> &fn) final {
            INTERNAL_DEBUG("removeSelfInto() svc {} removing svc {}", serviceId(), serviceIdOfOther);
            if constexpr (sizeof...(IFaces) > 0) {
                insertSelfInto2<sizeof...(IFaces), IFaces...>(keyOfInterfaceToInject, fn);
                // This is erased by the dependency manager
                _serviceIdsOfDependees.erase(serviceIdOfOther);
            }
        }

        Detail::DependencyChange dependencyOffline(ILifecycleManager* dependentService) final {
            INTERNAL_DEBUG("dependencyOffline() svc {}:{} dependent {}:{}", serviceId(), implementationName(), dependentService->serviceId(), dependentService->implementationName());
            auto const &interfaces = dependentService->getInterfaces();
            auto interested = Detail::DependencyChange::NOT_FOUND;

            for(auto const &interface : interfaces) {
                auto dep = _dependencies.find(interface);
                if (dep == _dependencies.end()) {
                    continue;
                }

                // dependency should not be marked as unsatisfied if there is at least one other of the same type present
                dep->satisfied--;
                if (dep->required && dep->satisfied == 0) {
                    interested = Detail::DependencyChange::FOUND_AND_STOP_ME;
                } else if(interested != Detail::DependencyChange::FOUND_AND_STOP_ME) {
                    interested = Detail::DependencyChange::FOUND;
                }

                removeSelfIntoDoubleDispatch(interface.interfaceNameHash, dependentService);
            }

            return interested;
        }

        [[nodiscard]]
        unordered_set<uint64_t> &getDependencies() noexcept final {
            return _serviceIdsOfInjectedDependencies;
        }

        [[nodiscard]]
        unordered_set<uint64_t> &getDependees() noexcept final {
            return _serviceIdsOfDependees;
        }

        void removeSelfIntoDoubleDispatch(uint64_t keyOfInterfaceToInject, ILifecycleManager* dependentService) {
            INTERNAL_DEBUG("removeSelfIntoDoubleDispatch() svc {} removing svc {}", serviceId(), dependentService->serviceId());
            auto dep = _registry._registrations.find(keyOfInterfaceToInject);

            if(dep != end(_registry._registrations)) {
                dependentService->removeSelfInto(keyOfInterfaceToInject, serviceId(), std::get<2>(dep->second));
                _serviceIdsOfInjectedDependencies.erase(dependentService->serviceId());
            }
        }

        [[nodiscard]]
        StartBehaviour start() final {
            bool canStart = _service.getState() != ServiceState::ACTIVE && _dependencies.allSatisfied();
            StartBehaviour ret = StartBehaviour::FAILED_DO_NOT_RETRY;
            if (canStart) {
                ret = _service.internal_start();
            }

            return ret;
        }

        [[nodiscard]]
        StartBehaviour stop() final {
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
            return _implementationName;
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
        const std::string_view _implementationName;
        std::vector<Dependency> _interfaces;
        DependencyRegister _registry;
        DependencyInfo _dependencies;
        ServiceType _service;
        unordered_set<uint64_t> _serviceIdsOfInjectedDependencies; // Services that this service depends on.
        unordered_set<uint64_t> _serviceIdsOfDependees; // services that depend on this service
    };

    template<class ServiceType, typename... IFaces>
    requires DerivedTemplated<ServiceType, Service>
    class LifecycleManager final : public ILifecycleManager {
    public:
        template <typename U = ServiceType> requires RequestsProperties<U>
        explicit LifecycleManager(std::vector<Dependency> interfaces, Properties&& properties, DependencyManager *mng) : _implementationName(typeName<ServiceType>()), _interfaces(std::move(interfaces)), _service(std::forward<Properties>(properties), mng) {
        }

        template <typename U = ServiceType> requires (!RequestsProperties<U>)
        explicit LifecycleManager(std::vector<Dependency> interfaces, Properties&& properties, DependencyManager *mng) : _implementationName(typeName<ServiceType>()), _interfaces(std::move(interfaces)), _service() {
            _service.setProperties(std::forward<Properties>(properties));
        }

        ~LifecycleManager() final = default;

        template<typename... Interfaces>
        [[nodiscard]]
        static std::unique_ptr<LifecycleManager<ServiceType, Interfaces...>> create(Properties&& properties, DependencyManager *mng, InterfacesList_t<Interfaces...>) {
            std::vector<Dependency> interfaces{};
            interfaces.reserve(sizeof...(Interfaces));
            (interfaces.emplace_back(typeNameHash<Interfaces>(), false, false),...);
            return std::make_unique<LifecycleManager<ServiceType, Interfaces...>>(std::move(interfaces), std::forward<Properties>(properties), mng);
        }

        Detail::DependencyChange dependencyOnline(ILifecycleManager* dependentService) final {
            return Detail::DependencyChange::NOT_FOUND;
        }

        Detail::DependencyChange dependencyOffline(ILifecycleManager* dependentService) final {
            return Detail::DependencyChange::NOT_FOUND;
        }

        [[nodiscard]]
        unordered_set<uint64_t> &getDependencies() noexcept final {
            return Detail::emptyDependencies;
        }

        [[nodiscard]]
        unordered_set<uint64_t> &getDependees() noexcept final {
            return _serviceIdsOfDependees;
        }

        [[nodiscard]]
        StartBehaviour start() final {
            return _service.internal_start();
        }

        [[nodiscard]]
        StartBehaviour stop() final {
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
            return _implementationName;
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

        [[nodiscard]] const std::vector<Dependency>& getInterfaces() const noexcept final {
            return _interfaces;
        }

        [[nodiscard]] Properties const & getProperties() const noexcept final {
            return _service._properties;
        }

        [[nodiscard]] DependencyRegister const * getDependencyRegistry() const noexcept final {
            return nullptr;
        }

        void insertSelfInto(uint64_t keyOfInterfaceToInject, uint64_t serviceIdOfOther, std::function<void(void*, IService*)> &fn) final {
            if constexpr (sizeof...(IFaces) > 0) {
                insertSelfInto2<sizeof...(IFaces), IFaces...>(keyOfInterfaceToInject, fn);
                _serviceIdsOfDependees.insert(serviceIdOfOther);
            }
        }

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

        void removeSelfInto(uint64_t keyOfInterfaceToInject, uint64_t serviceIdOfOther, std::function<void(void*, IService*)> &fn) final {
            INTERNAL_DEBUG("removeSelfInto2() svc {} removing svc {}", serviceId(), serviceIdOfOther);
            if constexpr (sizeof...(IFaces) > 0) {
                insertSelfInto2<sizeof...(IFaces), IFaces...>(keyOfInterfaceToInject, fn);
                // This is erased by the dependency manager
                _serviceIdsOfDependees.erase(serviceIdOfOther);
            }
        }

    private:
        const std::string_view _implementationName;
        std::vector<Dependency> _interfaces;
        ServiceType _service;
        unordered_set<uint64_t> _serviceIdsOfDependees; // services that depend on this service
    };
}
