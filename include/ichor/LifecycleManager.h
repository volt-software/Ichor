#pragma once

#include <string_view>
#include <memory>
#include <atomic>
#include <ranges>
#include <ichor/Service.h>
#include <ichor/interfaces/IFrameworkLogger.h>
#include <ichor/Common.h>
#include <ichor/Events.h>
#include <ichor/Dependency.h>
#include <ichor/DependencyInfo.h>
#include <ichor/DependencyRegister.h>

namespace Ichor {
    class ILifecycleManager {
    public:
        ICHOR_CONSTEXPR virtual ~ILifecycleManager() = default;
        ///
        /// \param dependentService
        /// \return true if dependency is registered in service, false if not
        ICHOR_CONSTEXPR virtual bool dependencyOnline(ILifecycleManager* dependentService) = 0;
        ///
        /// \param dependentService
        /// \return true if dependency is registered in service, false if not
        ICHOR_CONSTEXPR virtual bool dependencyOffline(ILifecycleManager* dependentService) = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual bool start() = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual bool stop() = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual std::string_view implementationName() const noexcept = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual uint64_t type() const noexcept = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual uint64_t serviceId() const noexcept = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual uint64_t getPriority() const noexcept = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual ServiceState getServiceState() const noexcept = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual const std::pmr::vector<Dependency>& getInterfaces() const noexcept = 0;

        // for some reason, returning a reference produces garbage??
        [[nodiscard]] ICHOR_CONSTEXPR virtual DependencyInfo const * getDependencyInfo() const noexcept = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual IchorProperties const * getProperties() const noexcept = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual IService* getServiceAsInterfacePointer() noexcept = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual DependencyRegister const * getDependencyRegistry() const noexcept = 0;
    };

    template<class ServiceType>
    requires DerivedTemplated<ServiceType, Service>
    class DependencyLifecycleManager final : public ILifecycleManager {
    public:
        explicit ICHOR_CONSTEXPR DependencyLifecycleManager(IFrameworkLogger *logger, std::string_view name, std::pmr::vector<Dependency> interfaces, IchorProperties&& properties, DependencyManager *mng, std::pmr::memory_resource *memResource) : _implementationName(name), _interfaces(std::move(interfaces)), _registry(mng), _dependencies(memResource), _satisfiedDependencies(memResource), _injectedDependencies(memResource), _service(_registry, std::forward<IchorProperties>(properties), mng), _logger(logger) {
            for(auto const &reg : _registry._registrations) {
                _dependencies.addDependency(std::get<0>(reg.second));
            }
        }

        ICHOR_CONSTEXPR ~DependencyLifecycleManager() final {
            ICHOR_LOG_TRACE(_logger, "destroying {}, id {}", typeName<ServiceType>(), _service.getServiceId());
            for(auto const &dep : _dependencies._dependencies) {
                // _manager is always injected in DependencyManager::create...Manager functions.
                _service._manager->template pushPrioritisedEvent<DependencyUndoRequestEvent>(_service.getServiceId(), getPriority(), Dependency{dep.interfaceNameHash, dep.required}, getProperties());
            }
        }

        template<Derived<IService>... Interfaces>
        [[nodiscard]]
        static std::shared_ptr<DependencyLifecycleManager<ServiceType>> create(IFrameworkLogger *logger, std::string_view name, IchorProperties&& properties, DependencyManager *mng, std::pmr::memory_resource *memResource, InterfacesList_t<Interfaces...>) {
            if (name.empty()) {
                name = typeName<ServiceType>();
            }

            std::pmr::vector<Dependency> interfaces{memResource};
            interfaces.reserve(sizeof...(Interfaces));
            (interfaces.emplace_back(typeNameHash<Interfaces>(), false),...);
            return std::allocate_shared<DependencyLifecycleManager<ServiceType>, std::pmr::polymorphic_allocator<>>(memResource, logger, name, std::move(interfaces), std::forward<IchorProperties>(properties), mng, memResource);
        }

        ICHOR_CONSTEXPR bool dependencyOnline(ILifecycleManager* dependentService) final {
            bool interested = false;
            auto const &interfaces = dependentService->getInterfaces();

            if(std::find(_injectedDependencies.begin(), _injectedDependencies.end(), dependentService->serviceId()) != _injectedDependencies.end()) {
                return interested;
            }

            for(auto const &interface : interfaces) {
                auto dep = _dependencies.find(interface);
                if (dep == _dependencies.end() || (dep->required && _satisfiedDependencies.contains(interface))) {
                    continue;
                }

                interested = true;
                injectIntoSelf(interface.interfaceNameHash, dependentService);
                if(dep->required) {
                    _satisfiedDependencies.addDependency(interface);
                }
            }

            if(interested) {
                _injectedDependencies.push_back(dependentService->serviceId());
            }

            return interested;
        }

        ICHOR_CONSTEXPR void injectIntoSelf(uint64_t keyOfInterfaceToInject, ILifecycleManager* dependentService) {
            auto dep = _registry._registrations.find(keyOfInterfaceToInject);

            if(dep != end(_registry._registrations)) {
                std::get<1>(dep->second)(dependentService->getServiceAsInterfacePointer());
            }
        }

        ICHOR_CONSTEXPR bool dependencyOffline(ILifecycleManager* dependentService) final {
            auto const &interfaces = dependentService->getInterfaces();
            bool interested = false;

            if(std::find(_injectedDependencies.begin(), _injectedDependencies.end(), dependentService->serviceId()) == _injectedDependencies.end()) {
                return interested;
            }

            for(auto const &interface : interfaces) {
                auto dep = _dependencies.find(interface);
                if (dep == _dependencies.end() || (dep->required && !_satisfiedDependencies.contains(interface))) {
                    continue;
                }

                if (dep->required) {
                    _satisfiedDependencies.removeDependency(interface);
                    interested = true;
                }

                removeSelfInto(interface.interfaceNameHash, dependentService);
            }

            std::erase(_injectedDependencies, dependentService->serviceId());

            return interested;
        }

        ICHOR_CONSTEXPR void removeSelfInto(uint64_t keyOfInterfaceToInject, ILifecycleManager* dependentService) {
            auto dep = _registry._registrations.find(keyOfInterfaceToInject);

            if(dep != end(_registry._registrations)) {
                std::get<2>(dep->second)(dependentService->getServiceAsInterfacePointer());
            }
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR bool start() final {
            bool canStart = _service.getState() != ServiceState::ACTIVE && _dependencies.requiredDependenciesSatisfied(_satisfiedDependencies);
            if (canStart) {
                if(_service.internal_start()) {
                    ICHOR_LOG_DEBUG(_logger, "Started {}", _implementationName);
                    return true;
                } else {
//                    ICHOR_LOG_DEBUG(_logger, "Couldn't start {}", _implementationName);
                }
            }

            return false;
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR bool stop() final {
            if(_service.internal_stop()) {
                ICHOR_LOG_DEBUG(_logger, "Stopped {}", _implementationName);
                return true;
            }

            ICHOR_LOG_DEBUG(_logger, "Couldn't stop {}", _implementationName);

            return false;
        }

        [[nodiscard]] ICHOR_CONSTEXPR std::string_view implementationName() const noexcept final {
            return _implementationName;
        }

        [[nodiscard]] ICHOR_CONSTEXPR uint64_t type() const noexcept final {
            return typeNameHash<ServiceType>();
        }

        [[nodiscard]] ICHOR_CONSTEXPR uint64_t serviceId() const noexcept final {
            return _service.getServiceId();
        }

        [[nodiscard]] ICHOR_CONSTEXPR uint64_t getPriority() const noexcept final {
            return _service.getServicePriority();
        }

        [[nodiscard]] ICHOR_CONSTEXPR ServiceType& getService() noexcept {
            return _service;
        }

        [[nodiscard]] ICHOR_CONSTEXPR ServiceState getServiceState() const noexcept final {
            return _service.getState();
        }

        [[nodiscard]] ICHOR_CONSTEXPR const std::pmr::vector<Dependency>& getInterfaces() const noexcept final {
            return _interfaces;
        }

        [[nodiscard]] ICHOR_CONSTEXPR DependencyInfo const * getDependencyInfo() const noexcept final {
            return &_dependencies;
        }

        [[nodiscard]] ICHOR_CONSTEXPR IchorProperties const * getProperties() const noexcept final {
            return &_service._properties;
        }

        [[nodiscard]] ICHOR_CONSTEXPR IService* getServiceAsInterfacePointer() noexcept final {
            return &_service;
        }

        [[nodiscard]] ICHOR_CONSTEXPR DependencyRegister const * getDependencyRegistry() const noexcept final {
            return &_registry;
        }

    private:
        const std::string_view _implementationName;
        std::pmr::vector<Dependency> _interfaces;
        DependencyRegister _registry;
        DependencyInfo _dependencies;
        DependencyInfo _satisfiedDependencies;
        std::pmr::vector<uint64_t> _injectedDependencies;
        ServiceType _service;
        IFrameworkLogger *_logger;
    };

    template<class ServiceType>
    requires DerivedTemplated<ServiceType, Service>
    class LifecycleManager final : public ILifecycleManager {
    public:
        template <typename U = ServiceType> requires RequestsProperties<U>
        explicit ICHOR_CONSTEXPR LifecycleManager(IFrameworkLogger *logger, std::string_view name, std::pmr::vector<Dependency> interfaces, IchorProperties&& properties, DependencyManager *mng) : _implementationName(name), _interfaces(std::move(interfaces)), _service(std::forward<IchorProperties>(properties), mng), _logger(logger) {
        }

        template <typename U = ServiceType> requires (!RequestsProperties<U>)
        explicit ICHOR_CONSTEXPR LifecycleManager(IFrameworkLogger *logger, std::string_view name, std::pmr::vector<Dependency> interfaces, IchorProperties&& properties, DependencyManager *mng) : _implementationName(name), _interfaces(std::move(interfaces)), _service(), _logger(logger) {
            _service.setProperties(std::forward<IchorProperties>(properties));
        }

        ICHOR_CONSTEXPR ~LifecycleManager() final = default;

        template<Derived<IService>... Interfaces>
        [[nodiscard]]
        static std::shared_ptr<LifecycleManager<ServiceType>> create(IFrameworkLogger *logger, std::string_view name, IchorProperties&& properties, DependencyManager *mng, std::pmr::memory_resource *memResource, InterfacesList_t<Interfaces...>) {
            if (name.empty()) {
                name = typeName<ServiceType>();
            }

            std::pmr::vector<Dependency> interfaces{memResource};
            interfaces.reserve(sizeof...(Interfaces));
            (interfaces.emplace_back(typeNameHash<Interfaces>(), false),...);
            return std::allocate_shared<LifecycleManager<ServiceType>, std::pmr::polymorphic_allocator<>>(memResource, logger, name, std::move(interfaces), std::forward<IchorProperties>(properties), mng);
        }

        ICHOR_CONSTEXPR bool dependencyOnline(ILifecycleManager* dependentService) final {
            return false;
        }

        ICHOR_CONSTEXPR bool dependencyOffline(ILifecycleManager* dependentService) final {
            return false;
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR bool start() final {
            if(_service.internal_start()) {
                ICHOR_LOG_DEBUG(_logger, "Started {}", _implementationName);
                return true;
            }

            ICHOR_LOG_DEBUG(_logger, "Couldn't start {}", _implementationName);

            return false;
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR bool stop() final {
            if(_service.internal_stop()) {
                ICHOR_LOG_DEBUG(_logger, "Stopped {}", _implementationName);
                return true;
            }

            ICHOR_LOG_DEBUG(_logger, "Couldn't stop {}", _implementationName);

            return false;
        }

        [[nodiscard]] ICHOR_CONSTEXPR std::string_view implementationName() const noexcept final {
            return _implementationName;
        }

        [[nodiscard]] ICHOR_CONSTEXPR uint64_t type() const noexcept final {
            return typeNameHash<ServiceType>();
        }

        [[nodiscard]] ICHOR_CONSTEXPR uint64_t serviceId() const noexcept final {
            return _service.getServiceId();
        }

        [[nodiscard]] ICHOR_CONSTEXPR ServiceType& getService() noexcept {
            return _service;
        }

        [[nodiscard]] ICHOR_CONSTEXPR uint64_t getPriority() const noexcept final {
            return _service.getServicePriority();
        }

        [[nodiscard]] ICHOR_CONSTEXPR ServiceState getServiceState() const noexcept final {
            return _service.getState();
        }

        [[nodiscard]] ICHOR_CONSTEXPR const std::pmr::vector<Dependency>& getInterfaces() const noexcept final {
            return _interfaces;
        }

        [[nodiscard]] ICHOR_CONSTEXPR DependencyInfo const * getDependencyInfo() const noexcept final {
            return nullptr;
        }

        [[nodiscard]] ICHOR_CONSTEXPR IchorProperties const * getProperties() const noexcept final {
            return &_service._properties;
        }

        [[nodiscard]] ICHOR_CONSTEXPR IService* getServiceAsInterfacePointer() noexcept final {
            return &_service;
        }

        [[nodiscard]] ICHOR_CONSTEXPR DependencyRegister const * getDependencyRegistry() const noexcept final {
            return nullptr;
        }

    private:
        const std::string_view _implementationName;
        std::pmr::vector<Dependency> _interfaces;
        ServiceType _service;
        IFrameworkLogger *_logger;
    };
}
