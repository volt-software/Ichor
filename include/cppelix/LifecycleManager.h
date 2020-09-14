#pragma once

#include <string_view>
#include <memory>
#include <atomic>
#include <unordered_set>
#include <ranges>
#include <cppelix/Service.h>
#include <cppelix/interfaces/IFrameworkLogger.h>
#include <cppelix/Common.h>
#include <cppelix/Events.h>
#include <cppelix/Dependency.h>

namespace Cppelix {
    enum class ServiceManagerState {
        INACTIVE,
        WAITING_FOR_REQUIRED,
        INSTANTIATED_AND_WAITING_FOR_REQUIRED,
        TRACKING_OPTIONAL
    };

    struct DependencyInfo final {

        CPPELIX_CONSTEXPR DependencyInfo() = default;
        CPPELIX_CONSTEXPR ~DependencyInfo() = default;
        CPPELIX_CONSTEXPR DependencyInfo(const DependencyInfo&) = delete;
        CPPELIX_CONSTEXPR DependencyInfo(DependencyInfo&&) noexcept = default;
        CPPELIX_CONSTEXPR DependencyInfo& operator=(const DependencyInfo&) = delete;
        CPPELIX_CONSTEXPR DependencyInfo& operator=(DependencyInfo&&) noexcept = default;

        template<class Interface>
        CPPELIX_CONSTEXPR void addDependency(bool required = true) {
            _dependencies.emplace_back(typeNameHash<Interface>(), Interface::version, required);
        }

        CPPELIX_CONSTEXPR void addDependency(Dependency dependency) {
            _dependencies.emplace_back(dependency);
        }

        template<class Interface>
        CPPELIX_CONSTEXPR void removeDependency() {
            std::erase_if(_dependencies, [](const auto& dep) noexcept { return dep.interfaceNameHash == typeNameHash<Interface>() && dep.interfaceVersion == Interface::version(); });
        }

        CPPELIX_CONSTEXPR void removeDependency(const Dependency &dependency) {
            std::erase_if(_dependencies, [&dependency](const auto& dep) noexcept { return dep.interfaceNameHash == dependency.interfaceNameHash && dep.interfaceVersion == dependency.interfaceVersion; });
        }

        template<class Interface>
        [[nodiscard]]
        CPPELIX_CONSTEXPR bool contains() const {
            return cend(_dependencies) != std::find_if(cbegin(_dependencies), cend(_dependencies), [](const auto& dep) noexcept { return dep.interfaceNameHash == typeNameHash<Interface>() && dep.interfaceVersion == Interface::version(); });
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool contains(const Dependency &dependency) const {
            return cend(_dependencies) != std::find_if(cbegin(_dependencies), cend(_dependencies), [&dependency](const auto& dep) noexcept { return dep.interfaceNameHash == dependency.interfaceNameHash && dep.interfaceVersion == dependency.interfaceVersion; });
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR auto find(const Dependency &dependency) const {
            return std::find_if(cbegin(_dependencies), cend(_dependencies), [&dependency](const auto& dep) noexcept { return dep.interfaceNameHash == dependency.interfaceNameHash && dep.interfaceVersion == dependency.interfaceVersion; });
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR auto end() {
            return std::end(_dependencies);
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR auto end() const {
            return std::cend(_dependencies);
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR size_t size() const {
            return _dependencies.size();
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool empty() const {
            return _dependencies.empty();
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR size_t amountRequired() const {
            return std::count_if(_dependencies.cbegin(), _dependencies.cend(), [](const auto &dep){ return dep.required; });
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR auto requiredDependencies() const {
            return _dependencies | std::ranges::views::filter([](auto &dep){ return dep.required; });
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR auto requiredDependenciesSatisfied(const DependencyInfo &satisfied) const {
            for(auto &requiredDep : requiredDependencies()) {
                if(!satisfied.contains(requiredDep)) {
                    return false;
                }
            }

            return true;
        }

        CPPELIX_CONSTEXPR std::vector<Dependency> _dependencies;
    };

    class DependencyRegister final {
    public:
        template<Derived<IService> Interface, Derived<Service> Impl>
        void registerDependency(Impl *svc, bool required, std::optional<CppelixProperties> props = {}) {
            InterfaceKey key{typeNameHash<Interface>(), Interface::version};
            if(_registrations.contains(key)) {
                throw std::runtime_error("Already registered interface");
            }

            _registrations.emplace(key, std::make_tuple(
                    Dependency{typeNameHash<Interface>(), Interface::version, required},
                    [svc](void* dep){ svc->addDependencyInstance(static_cast<Interface*>(dep)); },
                    [svc](void* dep){ svc->removeDependencyInstance(static_cast<Interface*>(dep)); },
                    std::move(props)));
        }

        std::unordered_map<InterfaceKey, std::tuple<Dependency, std::function<void(IService*)>, std::function<void(IService*)>, std::optional<CppelixProperties>>> _registrations;
    };

    class ILifecycleManager {
    public:
        CPPELIX_CONSTEXPR virtual ~ILifecycleManager() = default;
        ///
        /// \param dependentService
        /// \return true if started, false if not
        CPPELIX_CONSTEXPR virtual bool dependencyOnline(const std::shared_ptr<ILifecycleManager> &dependentService) = 0;
        ///
        /// \param dependentService
        /// \return true if stopped, false if not
        CPPELIX_CONSTEXPR virtual bool dependencyOffline(const std::shared_ptr<ILifecycleManager> &dependentService) = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual bool start() = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual bool stop() = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual std::string_view implementationName() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual uint64_t type() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual uint64_t serviceId() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual ServiceState getServiceState() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual const std::vector<Dependency>& getInterfaces() const = 0;

        // for some reason, returning a reference produces garbage??
        [[nodiscard]] CPPELIX_CONSTEXPR virtual DependencyInfo const * getDependencyInfo() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual CppelixProperties const * getProperties() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual IService* getServiceAsInterfacePointer() = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual DependencyRegister const * getDependencyRegistry() const = 0;
    };

    template<class ServiceType>
    requires Derived<ServiceType, Service>
    class DependencyLifecycleManager final : public ILifecycleManager {
    public:
        explicit CPPELIX_CONSTEXPR DependencyLifecycleManager(IFrameworkLogger *logger, std::string_view name, std::vector<Dependency> interfaces, CppelixProperties properties) : _implementationName(name), _interfaces(std::move(interfaces)), _registry(), _dependencies(), _satisfiedDependencies(), _service(_registry, std::move(properties)), _logger(logger) {
            for(const auto &reg : _registry._registrations) {
                _dependencies.addDependency(std::get<0>(reg.second));
            }
        }

        CPPELIX_CONSTEXPR ~DependencyLifecycleManager() final {
            LOG_TRACE(_logger, "destroying {}, id {}", typeName<ServiceType>(), _service.getServiceId());
            for(const auto &dep : _dependencies._dependencies) {
                // _manager is always injected in DependencyManager::create...Manager functions.
                _service._manager->template pushEvent<DependencyUndoRequestEvent>(_service.getServiceId(), nullptr, Dependency{dep.interfaceNameHash, dep.interfaceVersion, dep.required}, getProperties());
            }
        }

        template<Derived<IService>... Interfaces>
        [[nodiscard]]
        static std::shared_ptr<DependencyLifecycleManager<ServiceType>> create(IFrameworkLogger *logger, std::string_view name, CppelixProperties properties, InterfacesList_t<Interfaces...>) {
            if (name.empty()) {
                name = typeName<ServiceType>();
            }

            std::vector<Dependency> interfaces;
            interfaces.reserve(sizeof...(Interfaces));
            (interfaces.emplace_back(typeNameHash<Interfaces>(), Interfaces::version, false),...);
            auto mgr = std::make_shared<DependencyLifecycleManager<ServiceType>>(logger, name, std::move(interfaces), std::move(properties));
            return mgr;
        }

        CPPELIX_CONSTEXPR bool dependencyOnline(const std::shared_ptr<ILifecycleManager> &dependentService) final {
            const auto &interfaces = dependentService->getInterfaces();
            for(const auto &interface : interfaces) {
                auto dep = _dependencies.find(interface);
                if (dep == _dependencies.end() || _satisfiedDependencies.contains(interface)) {
                    continue;
                }

                injectIntoSelf(InterfaceKey{interface.interfaceNameHash, interface.interfaceVersion}, dependentService);
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

        CPPELIX_CONSTEXPR void injectIntoSelf(InterfaceKey keyOfInterfaceToInject, const std::shared_ptr<ILifecycleManager> &dependentService) {
            auto dep = _registry._registrations.find(keyOfInterfaceToInject);

            if(dep != end(_registry._registrations)) {
                std::get<1>(dep->second)(dependentService->getServiceAsInterfacePointer());
            }
        }

        CPPELIX_CONSTEXPR bool dependencyOffline(const std::shared_ptr<ILifecycleManager> &dependentService) final {
            const auto &interfaces = dependentService->getInterfaces();
            bool stopped = false;

            for(const auto &interface : interfaces) {
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

                removeSelfInto(InterfaceKey{interface.interfaceNameHash, interface.interfaceVersion}, dependentService);
            }

            return stopped;
        }

        CPPELIX_CONSTEXPR void removeSelfInto(InterfaceKey keyOfInterfaceToInject, const std::shared_ptr<ILifecycleManager> &dependentService) {
            auto dep = _registry._registrations.find(keyOfInterfaceToInject);

            if(dep != end(_registry._registrations)) {
                std::get<2>(dep->second)(dependentService->getServiceAsInterfacePointer());
            }
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool start() final {
            bool canStart = _service.getState() != ServiceState::ACTIVE && _dependencies.requiredDependenciesSatisfied(_satisfiedDependencies);
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
        CPPELIX_CONSTEXPR bool stop() final {
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

        [[nodiscard]] CPPELIX_CONSTEXPR std::string_view implementationName() const final {
            return _implementationName;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR uint64_t type() const final {
            return typeNameHash<ServiceType>();
        }

        [[nodiscard]] CPPELIX_CONSTEXPR uint64_t serviceId() const final {
            return _service.getServiceId();
        }

        [[nodiscard]] CPPELIX_CONSTEXPR ServiceType& getService() {
            return _service;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR ServiceState getServiceState() const final {
            return _service.getState();
        }

        [[nodiscard]] CPPELIX_CONSTEXPR const std::vector<Dependency>& getInterfaces() const final {
            return _interfaces;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR DependencyInfo const * getDependencyInfo() const final {
            return &_dependencies;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR CppelixProperties const * getProperties() const final {
            return &_service._properties;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR IService* getServiceAsInterfacePointer() final {
            return &_service;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR DependencyRegister const * getDependencyRegistry() const final {
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
        explicit CPPELIX_CONSTEXPR LifecycleManager(IFrameworkLogger *logger, std::string_view name, std::vector<Dependency> interfaces, CppelixProperties properties) : _implementationName(name), _interfaces(std::move(interfaces)), _service(), _logger(logger) {
            _service.setProperties(std::move(properties));
        }

        CPPELIX_CONSTEXPR ~LifecycleManager() final = default;

        template<Derived<IService>... Interfaces>
        [[nodiscard]]
        static std::shared_ptr<LifecycleManager<ServiceType>> create(IFrameworkLogger *logger, std::string_view name, CppelixProperties properties, InterfacesList_t<Interfaces...>) {
            if (name.empty()) {
                name = typeName<ServiceType>();
            }

            std::vector<Dependency> interfaces;
            interfaces.reserve(sizeof...(Interfaces));
            (interfaces.emplace_back(typeNameHash<Interfaces>(), Interfaces::version, false),...);
            auto mgr = std::make_shared<LifecycleManager<ServiceType>>(logger, name, std::move(interfaces), std::move(properties));
            return mgr;
        }

        CPPELIX_CONSTEXPR bool dependencyOnline(const std::shared_ptr<ILifecycleManager> &dependentService) final {
            return false;
        }

        CPPELIX_CONSTEXPR bool dependencyOffline(const std::shared_ptr<ILifecycleManager> &dependentService) final {
            return false;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool start() final {
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
        CPPELIX_CONSTEXPR bool stop() final {
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

        [[nodiscard]] CPPELIX_CONSTEXPR std::string_view implementationName() const final {
            return _implementationName;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR uint64_t type() const final {
            return typeNameHash<ServiceType>();
        }

        [[nodiscard]] CPPELIX_CONSTEXPR uint64_t serviceId() const final {
            return _service.getServiceId();
        }

        [[nodiscard]] CPPELIX_CONSTEXPR ServiceType& getService() {
            return _service;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR ServiceState getServiceState() const final {
            return _service.getState();
        }

        [[nodiscard]] CPPELIX_CONSTEXPR const std::vector<Dependency>& getInterfaces() const final {
            return _interfaces;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR DependencyInfo const * getDependencyInfo() const final {
            return nullptr;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR CppelixProperties const * getProperties() const final {
            return &_service._properties;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR IService* getServiceAsInterfacePointer() final {
            return &_service;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR DependencyRegister const * getDependencyRegistry() const final {
            return nullptr;
        }

    private:
        const std::string_view _implementationName;
        std::vector<Dependency> _interfaces;
        ServiceType _service;
        IFrameworkLogger *_logger;
    };
}
