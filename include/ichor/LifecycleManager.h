#pragma once

#include <string_view>
#include <memory>
#include <atomic>
#include <unordered_set>
#include <ranges>
#include <ichor/Service.h>
#include <ichor/interfaces/IFrameworkLogger.h>
#include <ichor/Common.h>
#include <ichor/Events.h>
#include <ichor/Dependency.h>

namespace Ichor {
    struct DependencyInfo final {

        ICHOR_CONSTEXPR DependencyInfo() = default;
        ICHOR_CONSTEXPR ~DependencyInfo() = default;
        ICHOR_CONSTEXPR DependencyInfo(const DependencyInfo&) = delete;
        ICHOR_CONSTEXPR DependencyInfo(DependencyInfo&&) noexcept = default;
        ICHOR_CONSTEXPR DependencyInfo& operator=(const DependencyInfo&) = delete;
        ICHOR_CONSTEXPR DependencyInfo& operator=(DependencyInfo&&) noexcept = default;

        template<class Interface>
        ICHOR_CONSTEXPR void addDependency(bool required = true) {
            _dependencies.emplace_back(typeNameHash<Interface>(), required);
        }

        ICHOR_CONSTEXPR void addDependency(Dependency dependency) {
            _dependencies.emplace_back(dependency);
        }

        template<class Interface>
        ICHOR_CONSTEXPR void removeDependency() {
            std::erase_if(_dependencies, [](auto const& dep) noexcept { return dep.interfaceNameHash == typeNameHash<Interface>(); });
        }

        ICHOR_CONSTEXPR void removeDependency(const Dependency &dependency) {
            std::erase_if(_dependencies, [&dependency](auto const& dep) noexcept { return dep.interfaceNameHash == dependency.interfaceNameHash; });
        }

        template<class Interface>
        [[nodiscard]]
        ICHOR_CONSTEXPR bool contains() const {
            return cend(_dependencies) != std::find_if(cbegin(_dependencies), cend(_dependencies), [](auto const& dep) noexcept { return dep.interfaceNameHash == typeNameHash<Interface>(); });
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR bool contains(const Dependency &dependency) const {
            return cend(_dependencies) != std::find_if(cbegin(_dependencies), cend(_dependencies), [&dependency](auto const& dep) noexcept { return dep.interfaceNameHash == dependency.interfaceNameHash; });
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR auto find(const Dependency &dependency) const {
            return std::find_if(cbegin(_dependencies), cend(_dependencies), [&dependency](auto const& dep) noexcept { return dep.interfaceNameHash == dependency.interfaceNameHash; });
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR auto end() {
            return std::end(_dependencies);
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR auto end() const {
            return std::cend(_dependencies);
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR size_t size() const {
            return _dependencies.size();
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR bool empty() const {
            return _dependencies.empty();
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR size_t amountRequired() const {
            return std::count_if(_dependencies.cbegin(), _dependencies.cend(), [](auto const &dep){ return dep.required; });
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR auto requiredDependencies() const {
            return _dependencies | std::ranges::views::filter([](auto &dep){ return dep.required; });
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR auto requiredDependenciesSatisfied(const DependencyInfo &satisfied) const {
            for(auto &requiredDep : requiredDependencies()) {
                if(!satisfied.contains(requiredDep)) {
                    return false;
                }
            }

            return true;
        }

        ICHOR_CONSTEXPR std::vector<Dependency> _dependencies;
    };

    class DependencyRegister final {
    public:
        template<Derived<IService> Interface, Derived<Service> Impl>
        void registerDependency(Impl *svc, bool required, std::optional<IchorProperties> props = {}) {
            if(_registrations.contains(typeNameHash<Interface>())) {
                throw std::runtime_error("Already registered interface");
            }

            _registrations.emplace(typeNameHash<Interface>(), std::make_tuple(
                    Dependency{typeNameHash<Interface>(), required},
                    [svc](void* dep){ svc->addDependencyInstance(static_cast<Interface*>(dep)); },
                    [svc](void* dep){ svc->removeDependencyInstance(static_cast<Interface*>(dep)); },
                    std::move(props)));
        }

        std::unordered_map<uint64_t, std::tuple<Dependency, std::function<void(IService*)>, std::function<void(IService*)>, std::optional<IchorProperties>>> _registrations;
    };

    class ILifecycleManager {
    public:
        ICHOR_CONSTEXPR virtual ~ILifecycleManager() = default;
        ///
        /// \param dependentService
        /// \return true if started, false if not
        ICHOR_CONSTEXPR virtual bool dependencyOnline(const std::shared_ptr<ILifecycleManager> &dependentService) = 0;
        ///
        /// \param dependentService
        /// \return true if stopped, false if not
        ICHOR_CONSTEXPR virtual bool dependencyOffline(const std::shared_ptr<ILifecycleManager> &dependentService) = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual bool start() = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual bool stop() = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual std::string_view implementationName() const noexcept = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual uint64_t type() const noexcept = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual uint64_t serviceId() const noexcept = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual uint64_t getPriority() const noexcept = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual ServiceState getServiceState() const noexcept = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual const std::vector<Dependency>& getInterfaces() const noexcept = 0;

        // for some reason, returning a reference produces garbage??
        [[nodiscard]] ICHOR_CONSTEXPR virtual DependencyInfo const * getDependencyInfo() const noexcept = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual IchorProperties const * getProperties() const noexcept = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual IService* getServiceAsInterfacePointer() noexcept = 0;
        [[nodiscard]] ICHOR_CONSTEXPR virtual DependencyRegister const * getDependencyRegistry() const noexcept = 0;
    };

    template<class ServiceType>
    requires Derived<ServiceType, Service>
    class DependencyLifecycleManager final : public ILifecycleManager {
    public:
        explicit ICHOR_CONSTEXPR DependencyLifecycleManager(IFrameworkLogger *logger, std::string_view name, std::vector<Dependency> interfaces, IchorProperties properties, DependencyManager *mng) : _implementationName(name), _interfaces(std::move(interfaces)), _registry(), _dependencies(), _satisfiedDependencies(), _service(_registry, std::move(properties), mng), _logger(logger) {
            for(auto const &reg : _registry._registrations) {
                _dependencies.addDependency(std::get<0>(reg.second));
            }
        }

        ICHOR_CONSTEXPR ~DependencyLifecycleManager() final {
            LOG_TRACE(_logger, "destroying {}, id {}", typeName<ServiceType>(), _service.getServiceId());
            for(auto const &dep : _dependencies._dependencies) {
                // _manager is always injected in DependencyManager::create...Manager functions.
                _service._manager->template pushPrioritisedEvent<DependencyUndoRequestEvent>(_service.getServiceId(), getPriority(), nullptr, Dependency{dep.interfaceNameHash, dep.required}, getProperties());
            }
        }

        template<Derived<IService>... Interfaces>
        [[nodiscard]]
        static std::shared_ptr<DependencyLifecycleManager<ServiceType>> create(IFrameworkLogger *logger, std::string_view name, IchorProperties properties, DependencyManager *mng, std::pmr::memory_resource *memResource, InterfacesList_t<Interfaces...>) {
            if (name.empty()) {
                name = typeName<ServiceType>();
            }

            std::vector<Dependency> interfaces;
            interfaces.reserve(sizeof...(Interfaces));
            (interfaces.emplace_back(typeNameHash<Interfaces>(), false),...);
            auto mgr = std::allocate_shared<DependencyLifecycleManager<ServiceType>, std::pmr::polymorphic_allocator<>>(memResource, logger, name, std::move(interfaces), std::move(properties), mng);
            return mgr;
        }

        ICHOR_CONSTEXPR bool dependencyOnline(const std::shared_ptr<ILifecycleManager> &dependentService) final {
            auto const &interfaces = dependentService->getInterfaces();
            for(auto const &interface : interfaces) {
                auto dep = _dependencies.find(interface);
                if (dep == _dependencies.end() || _satisfiedDependencies.contains(interface)) {
                    continue;
                }

                injectIntoSelf(interface.interfaceNameHash, dependentService);
                if(dep->required) {
                    _satisfiedDependencies.addDependency(interface);

                    bool canStart = _dependencies.requiredDependenciesSatisfied(_satisfiedDependencies);
                    if (canStart) {
                        if (!_service.internal_start()) {
                            LOG_ERROR(_logger, "Couldn't start service {}", _implementationName);
                            return false;
                        }

                        LOG_DEBUG(_logger, "Started {}", _implementationName);
                        return true;
                    }
                }
            }

            return false;
        }

        ICHOR_CONSTEXPR void injectIntoSelf(uint64_t keyOfInterfaceToInject, const std::shared_ptr<ILifecycleManager> &dependentService) {
            auto dep = _registry._registrations.find(keyOfInterfaceToInject);

            if(dep != end(_registry._registrations)) {
                std::get<1>(dep->second)(dependentService->getServiceAsInterfacePointer());
            }
        }

        ICHOR_CONSTEXPR bool dependencyOffline(const std::shared_ptr<ILifecycleManager> &dependentService) final {
            auto const &interfaces = dependentService->getInterfaces();
            bool stopped = false;

            for(auto const &interface : interfaces) {
                auto dep = _dependencies.find(interface);
                if (dep == _dependencies.end() || (dep->required && !_satisfiedDependencies.contains(interface))) {
                    continue;
                }

                if (dep->required) {
                    _satisfiedDependencies.removeDependency(interface);
                    if (_service.getState() == ServiceState::ACTIVE) {
                        bool shouldStop = !_dependencies.requiredDependenciesSatisfied(_satisfiedDependencies);

                        if (shouldStop) {
                            if (!_service.internal_stop()) {
                                LOG_ERROR(_logger, "Couldn't stop service {}", _implementationName);
                            } else {
                                LOG_DEBUG(_logger, "stopped {}", _implementationName);
                                stopped = true;
                            }
                        }
                    }
                }

                removeSelfInto(interface.interfaceNameHash, dependentService);
            }

            return stopped;
        }

        ICHOR_CONSTEXPR void removeSelfInto(uint64_t keyOfInterfaceToInject, const std::shared_ptr<ILifecycleManager> &dependentService) {
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
                    LOG_DEBUG(_logger, "Started {}", _implementationName);
                    return true;
                } else {
//                    LOG_DEBUG(_logger, "Couldn't start {}", _implementationName);
                }
            }

            return false;
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR bool stop() final {
            if(_service.getState() == ServiceState::ACTIVE) {
                if(_service.internal_stop()) {
                    LOG_DEBUG(_logger, "Stopped {}", _implementationName);
                    return true;
                } else {
                    LOG_DEBUG(_logger, "Couldn't stop {}", _implementationName);
                }
            }

            return true;
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

        [[nodiscard]] ICHOR_CONSTEXPR const std::vector<Dependency>& getInterfaces() const noexcept final {
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
        std::vector<Dependency> _interfaces;
        DependencyRegister _registry;
        DependencyInfo _dependencies;
        DependencyInfo _satisfiedDependencies;
        ServiceType _service;
        IFrameworkLogger *_logger;
    };

    template<class ServiceType>
    requires Derived<ServiceType, Service>
    class LifecycleManager final : public ILifecycleManager {
    public:
        template <typename U = ServiceType> requires RequestsProperties<U>
        explicit ICHOR_CONSTEXPR LifecycleManager(IFrameworkLogger *logger, std::string_view name, std::vector<Dependency> interfaces, IchorProperties properties, DependencyManager *mng) : _implementationName(name), _interfaces(std::move(interfaces)), _service(std::move(properties), mng), _logger(logger) {
        }

        template <typename U = ServiceType> requires (!RequestsProperties<U>)
        explicit ICHOR_CONSTEXPR LifecycleManager(IFrameworkLogger *logger, std::string_view name, std::vector<Dependency> interfaces, IchorProperties properties, DependencyManager *mng) : _implementationName(name), _interfaces(std::move(interfaces)), _service(), _logger(logger) {
            _service.setProperties(std::move(properties));
        }

        ICHOR_CONSTEXPR ~LifecycleManager() final = default;

        template<Derived<IService>... Interfaces>
        [[nodiscard]]
        static std::shared_ptr<LifecycleManager<ServiceType>> create(IFrameworkLogger *logger, std::string_view name, IchorProperties properties, DependencyManager *mng, std::pmr::memory_resource *memResource, InterfacesList_t<Interfaces...>) {
            if (name.empty()) {
                name = typeName<ServiceType>();
            }

            std::vector<Dependency> interfaces;
            interfaces.reserve(sizeof...(Interfaces));
            (interfaces.emplace_back(typeNameHash<Interfaces>(), false),...);
            auto mgr = std::allocate_shared<LifecycleManager<ServiceType>, std::pmr::polymorphic_allocator<>>(memResource, logger, name, std::move(interfaces), std::move(properties), mng);
            return mgr;
        }

        ICHOR_CONSTEXPR bool dependencyOnline(const std::shared_ptr<ILifecycleManager> &dependentService) final {
            return false;
        }

        ICHOR_CONSTEXPR bool dependencyOffline(const std::shared_ptr<ILifecycleManager> &dependentService) final {
            return false;
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR bool start() final {
            bool canStart = _service.getState() != ServiceState::ACTIVE;
            if (canStart) {
                if(_service.internal_start()) {
                    LOG_DEBUG(_logger, "Started {}", _implementationName);
                    return true;
                } else {
                    LOG_DEBUG(_logger, "Couldn't start {}", _implementationName);
                }
            }

            return false;
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR bool stop() final {
            if(_service.getState() == ServiceState::ACTIVE) {
                if(_service.internal_stop()) {
                    LOG_DEBUG(_logger, "Stopped {}", _implementationName);
                    return true;
                } else {
                    LOG_DEBUG(_logger, "Couldn't stop {}", _implementationName);
                }
            }

            return true;
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

        [[nodiscard]] ICHOR_CONSTEXPR const std::vector<Dependency>& getInterfaces() const noexcept final {
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
        std::vector<Dependency> _interfaces;
        ServiceType _service;
        IFrameworkLogger *_logger;
    };
}
