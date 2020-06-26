#pragma once

#include <string_view>
#include <memory>
#include <atomic>
#include <unordered_set>
#include <ranges>
#include "Service.h"
#include "interfaces/IFrameworkLogger.h"
#include "Common.h"
#include "Filter.h"
#include "Property.h"

namespace Cppelix {
    enum class ServiceManagerState {
        INACTIVE,
        WAITING_FOR_REQUIRED,
        INSTANTIATED_AND_WAITING_FOR_REQUIRED,
        TRACKING_OPTIONAL
    };

    struct Dependency {
        CPPELIX_CONSTEXPR Dependency(uint64_t _interfaceNameHash, InterfaceVersion _interfaceVersion, bool _required) noexcept : interfaceNameHash(_interfaceNameHash), interfaceVersion(_interfaceVersion), required(_required) {}
        CPPELIX_CONSTEXPR Dependency(const Dependency &other) noexcept = delete;
        CPPELIX_CONSTEXPR Dependency(Dependency &&other) noexcept = default;
        CPPELIX_CONSTEXPR Dependency& operator=(const Dependency &other) noexcept = delete;
        CPPELIX_CONSTEXPR Dependency& operator=(Dependency &&other) noexcept = default;
        CPPELIX_CONSTEXPR bool operator==(const Dependency &other) const noexcept {
            return interfaceNameHash == other.interfaceNameHash && interfaceVersion == other.interfaceVersion && required == other.required;
        }

        uint64_t interfaceNameHash;
        InterfaceVersion interfaceVersion;
        bool required;
    };

    struct DependencyInfo {

        CPPELIX_CONSTEXPR DependencyInfo() : _dependencies() {}

        template<class Interface>
        CPPELIX_CONSTEXPR void addDependency(bool required = true) {
            _dependencies.emplace_back(typeNameHash<Interface>(), Interface::version, required);
        }

        CPPELIX_CONSTEXPR void addDependency(Dependency dependency) {
            _dependencies.emplace_back(std::move(dependency));
        }

        template<class Interface>
        CPPELIX_CONSTEXPR void removeDependency() {
            std::erase(std::remove_if(begin(_dependencies), end(_dependencies), [](const auto& dep) noexcept { return dep.interfaceNameHash == typeNameHash<Interface>() && dep.interfaceVersion == Interface::version(); }), end(_dependencies));
        }

        CPPELIX_CONSTEXPR void removeDependency(const Dependency &dependency) {
            _dependencies.erase(std::remove_if(begin(_dependencies), end(_dependencies), [&dependency](const auto& dep) noexcept { return dep.interfaceNameHash == dependency.interfaceNameHash && dep.interfaceVersion == dependency.interfaceVersion; }), end(_dependencies));
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
        CPPELIX_CONSTEXPR size_t size() const noexcept {
            return _dependencies.size();
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR size_t amountRequired() const noexcept {
            return std::count_if(_dependencies.cbegin(), _dependencies.cend(), [](const auto &dep){ return dep.required; });
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR auto requiredDependencies() const {
            return _dependencies | std::ranges::views::filter([](auto &dep){ return dep.required; });
        }

        CPPELIX_CONSTEXPR std::vector<Dependency> _dependencies;
    };

    class LifecycleManager {
    public:
        CPPELIX_CONSTEXPR virtual ~LifecycleManager() = default;
        CPPELIX_CONSTEXPR virtual void dependencyOnline(std::shared_ptr<LifecycleManager> dependentService) = 0;
        CPPELIX_CONSTEXPR virtual void dependencyOffline(std::shared_ptr<LifecycleManager> dependentService) = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual bool start() = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual bool stop() = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual bool shouldStart() = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual bool shouldStop() = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual std::string_view name() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual uint64_t type() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual uint64_t serviceId() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual ServiceState getServiceState() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual Dependency getSelfAsDependency() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual void* getServicePointer() = 0;

        template <typename T>
        [[nodiscard]] CPPELIX_CONSTEXPR T* getService() {
            return static_cast<T*>(getServicePointer());
        }
    };

    template<class Interface, class ServiceType, typename... Dependencies>
    requires Derived<ServiceType, Service>
    class DependencyServiceLifecycleManager : public LifecycleManager {
    public:
        explicit CPPELIX_CONSTEXPR DependencyServiceLifecycleManager(IFrameworkLogger *logger, std::string_view name, uint64_t nameHash, CppelixProperties properties) : _name(name), _nameHash(nameHash), _properties(std::move(properties)), _dependencies(), _satisfiedDependencies(), _service(), _logger(logger) {
        }

        CPPELIX_CONSTEXPR ~DependencyServiceLifecycleManager() final = default;

        template<typename... Required, typename... Optional>
        [[nodiscard]]
        static std::shared_ptr<DependencyServiceLifecycleManager<Interface, ServiceType, Required..., Optional...>> create(IFrameworkLogger *logger, std::string_view name, CppelixProperties properties, RequiredList_t<Required...>, OptionalList_t<Optional...>) {
            if (name.empty()) {
                name = typeName<Interface>();
            }

            auto mgr = std::make_shared<DependencyServiceLifecycleManager<Interface, ServiceType, Required..., Optional...>>(logger, name, typeNameHash<Interface>(), std::move(properties));
            mgr->template setupDependencies<Optional...>(false);
            mgr->template setupDependencies<Required...>(true);
            return mgr;
        }

        //stupid bug where it complains about accessing private variables.
        template <typename... TempDependencies>
        CPPELIX_CONSTEXPR void setupDependencies([[maybe_unused]] bool required) {
            (_dependencies.template addDependency<TempDependencies>(required), ...);
        }

        CPPELIX_CONSTEXPR void dependencyOnline(std::shared_ptr<LifecycleManager> dependentService) final {
            auto dependency = dependentService->getSelfAsDependency();
            if(!_dependencies.contains(dependency) || _satisfiedDependencies.contains(dependency)) {
                return;
            }

            injectSelfInto<Dependencies...>(dependency.interfaceNameHash, dependentService);
            _satisfiedDependencies.addDependency(std::move(dependency));

            if(_dependencies.amountRequired() == _satisfiedDependencies.amountRequired()) {
                if(!_service.internal_start()) {
                    LOG_ERROR(_logger, "Couldn't start service {}", _name);
                }
            }
        }

        template<class Interface0, class ...Interfaces>
        CPPELIX_CONSTEXPR void injectSelfInto(uint64_t hashOfInterfaceToInject, std::shared_ptr<LifecycleManager> dependentService) {
            if (typeNameHash<Interface0>() == hashOfInterfaceToInject) {
                _service.addDependencyInstance(static_cast<Interface0*>(dependentService->getServicePointer()));
            } else {
                if constexpr (sizeof...(Interfaces) > 0) {
                    injectSelfInto<Interfaces...>(hashOfInterfaceToInject, dependentService);
                }
            }
        }

        CPPELIX_CONSTEXPR void dependencyOffline(std::shared_ptr<LifecycleManager> dependentService) final {
            auto dependency = dependentService->getSelfAsDependency();
            if(!_dependencies.contains(dependency) || !_satisfiedDependencies.contains(dependency)) {
                return;
            }

            removeSelfInto<Dependencies...>(dependency.interfaceNameHash, dependentService);
            _satisfiedDependencies.removeDependency(dependency);

            if(_dependencies.amountRequired() != _satisfiedDependencies.amountRequired()) {
                if(!_service.internal_stop()) {
                    LOG_ERROR(_logger, "Couldn't stop service {}", _name);
                }
            }
        }

        template<class Interface0, class ...Interfaces>
        CPPELIX_CONSTEXPR void removeSelfInto(uint64_t hashOfInterfaceToInject, std::shared_ptr<LifecycleManager> dependentService) {
            if (typeNameHash<Interface0>() == hashOfInterfaceToInject) {
                _service.removeDependencyInstance(static_cast<Interface0*>(dependentService->getServicePointer()));
            } else {
                if constexpr (sizeof...(Interfaces) > 0) {
                    removeSelfInto<Interfaces...>(hashOfInterfaceToInject, dependentService);
                }
            }
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool start() final {
            if(_dependencies.amountRequired() == _satisfiedDependencies.size() && _service.getState() == ServiceState::INSTALLED) {
                return _service.internal_start();
            }

            return false;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool stop() final {
            if(_service.getState() == ServiceState::ACTIVE) {
                return _service.internal_stop();
            }

            return true;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool shouldStart() final {
            if(_service.getState() == ServiceState::ACTIVE) {
                return false;
            }

            for(const auto &dep : _dependencies.requiredDependencies()) {
                if(!_satisfiedDependencies.contains(dep)) {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool shouldStop() final {
            if(_service.getState() != ServiceState::ACTIVE) {
                return false;
            }

            for(const auto &dep : _dependencies.requiredDependencies()) {
                if(!_satisfiedDependencies.contains(dep)) {
                    return true;
                }
            }

            return false;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR std::string_view name() const final {
            return _name;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR uint64_t type() const final {
            return typeNameHash<ServiceType>();
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR uint64_t serviceId() const final {
            return _service.getServiceId();
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR ServiceType& getService() {
            return _service;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR ServiceState getServiceState() const final {
            return _service.getState();
        }

        [[nodiscard]] CPPELIX_CONSTEXPR Dependency getSelfAsDependency() const final {
            return Dependency{_nameHash, Interface::version, false};
        }

        [[nodiscard]] CPPELIX_CONSTEXPR void* getServicePointer() final {
            return &_service;
        }

    private:
        const std::string_view _name;
        const uint64_t _nameHash;
        CppelixProperties _properties;
        DependencyInfo _dependencies;
        DependencyInfo _satisfiedDependencies;
        ServiceType _service;
        IFrameworkLogger *_logger;
    };

    template<class Interface, class ServiceType>
    requires Derived<ServiceType, Service>
    class ServiceLifecycleManager : public LifecycleManager {
    public:
        explicit CPPELIX_CONSTEXPR ServiceLifecycleManager(std::string_view name, uint64_t nameHash, CppelixProperties properties) : _name(name), _nameHash(nameHash), _properties(std::move(properties)), _service(), _logger(&_service) {
            static_assert(std::is_same_v<Interface, IFrameworkLogger>, "Can only use this constructor if ServiceLifecycleManager is for IFrameworkLogger!");
        }

        explicit CPPELIX_CONSTEXPR ServiceLifecycleManager(IFrameworkLogger *logger, std::string_view name, uint64_t nameHash, CppelixProperties properties) : _name(name), _nameHash(nameHash), _properties(std::move(properties)), _service(), _logger(logger) {}

        CPPELIX_CONSTEXPR ~ServiceLifecycleManager() final = default;

        [[nodiscard]]
        static std::shared_ptr<ServiceLifecycleManager<Interface, ServiceType>> create(IFrameworkLogger *logger, std::string_view name, CppelixProperties properties) {
            if (name.empty()) {
                name = typeName<Interface>();
            }


            if constexpr (std::is_same_v<Interface, IFrameworkLogger>) {
                return std::make_shared<ServiceLifecycleManager<Interface, ServiceType>>(name, typeNameHash<Interface>(), std::move(properties));
            }
            return std::make_shared<ServiceLifecycleManager<Interface, ServiceType>>(logger, name, typeNameHash<Interface>(), std::move(properties));
        }

        CPPELIX_CONSTEXPR void dependencyOnline(std::shared_ptr<LifecycleManager> dependentService) final {
        }

        CPPELIX_CONSTEXPR void dependencyOffline(std::shared_ptr<LifecycleManager> dependentService) final {
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool start() final {
            if(_service.getState() == ServiceState::INSTALLED) {
                return _service.internal_start();
            }

            return false;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool stop() final {
            if(_service.getState() == ServiceState::ACTIVE) {
                return _service.internal_stop();
            }

            return false;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool shouldStart() final {
            if(_service.getState() == ServiceState::ACTIVE) {
                return false;
            }

            return true;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool shouldStop() final {
            return false;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR std::string_view name() const final {
            return _name;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR uint64_t type() const final {
            return typeNameHash<ServiceType>();
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR uint64_t serviceId() const final {
            return _service.getServiceId();
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR ServiceType& getService() {
            return _service;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR ServiceState getServiceState() const final {
            return _service.getState();
        }

        [[nodiscard]] CPPELIX_CONSTEXPR Dependency getSelfAsDependency() const final {
            return Dependency{typeNameHash<Interface>(), Interface::version, false};
        }

        [[nodiscard]] CPPELIX_CONSTEXPR void* getServicePointer() final {
            return &_service;
        }

    private:
        const std::string_view _name;
        uint64_t _nameHash;
        CppelixProperties _properties;
        ServiceType _service;
        IFrameworkLogger *_logger;
    };
}
