#pragma once

#include <string_view>
#include <memory>
#include <atomic>
#include <unordered_set>
#include <ranges>
#include "Service.h"
#include "interfaces/IFrameworkLogger.h"
#include "Common.h"
#include "Events.h"
#include "Dependency.h"

namespace Cppelix {
    enum class ServiceManagerState {
        INACTIVE,
        WAITING_FOR_REQUIRED,
        INSTANTIATED_AND_WAITING_FOR_REQUIRED,
        TRACKING_OPTIONAL
    };

    struct DependencyInfo {

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
            _dependencies.emplace_back(std::move(dependency));
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
        CPPELIX_CONSTEXPR size_t size() const {
            return _dependencies.size();
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
        [[nodiscard]] CPPELIX_CONSTEXPR virtual Dependency getSelfAsDependency() const = 0;

        // for some reason, returning a reference produces garbage??
        [[nodiscard]] CPPELIX_CONSTEXPR virtual DependencyInfo const * getDependencyInfo() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual CppelixProperties const * getProperties() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual void* getInterfacePointer() = 0;

        template <typename T>
        requires Derived<T, IService> && std::is_abstract_v<T>
        [[nodiscard]] CPPELIX_CONSTEXPR T* getInterface() {
            return static_cast<T*>(getInterfacePointer());
        }
    };

    template<class Interface, class ServiceType, typename... Dependencies>
    requires Derived<ServiceType, Service> && Derived<ServiceType, Interface>
    class LifecycleManager final : public ILifecycleManager {
    public:
        explicit CPPELIX_CONSTEXPR LifecycleManager(IFrameworkLogger *logger, std::string_view name, uint64_t nameHash, CppelixProperties properties) : _implementationName(name), _nameHash(nameHash), _dependencies(), _satisfiedDependencies(), _service(), _logger(logger) {
            _service.setProperties(std::move(properties));
        }

        CPPELIX_CONSTEXPR ~LifecycleManager() final {
            // _manager is always injected in DependencyManager::create...Manager functions.
            (_service._manager->template pushEvent<DependencyUndoRequestEvent>(_service.getServiceId(), nullptr, Dependency{typeNameHash<Dependencies>(), Dependencies::version, false}, getProperties()), ...);
        }

        template<typename... Required, typename... Optional>
        [[nodiscard]]
        static std::shared_ptr<LifecycleManager<Interface, ServiceType, Required..., Optional...>> create(IFrameworkLogger *logger, std::string_view name, CppelixProperties properties, RequiredList_t<Required...>, OptionalList_t<Optional...>) {
            if (name.empty()) {
                name = typeName<Interface>();
            }

            auto mgr = std::make_shared<LifecycleManager<Interface, ServiceType, Required..., Optional...>>(logger, name, typeNameHash<Interface>(), std::move(properties));
            mgr->template setupDependencies<Optional...>(false);
            mgr->template setupDependencies<Required...>(true);
            return mgr;
        }

        // stupid bug where it complains about accessing private variables.
        template <typename... TempDependencies>
        CPPELIX_CONSTEXPR void setupDependencies([[maybe_unused]] bool required) {
            (_dependencies.template addDependency<TempDependencies>(required), ...);
        }

        CPPELIX_CONSTEXPR bool dependencyOnline(const std::shared_ptr<ILifecycleManager> &dependentService) final {
            auto dependency = dependentService->getSelfAsDependency();
            if(!_dependencies.contains(dependency) || _satisfiedDependencies.contains(dependency)) {
                return false;
            }

            injectIntoSelf<Dependencies...>(dependency.interfaceNameHash, dependentService);
            _satisfiedDependencies.addDependency(std::move(dependency));

            bool canStart = _service.getState() != ServiceState::ACTIVE && _dependencies.requiredDependenciesSatisfied(_satisfiedDependencies);
            if(canStart) {
                if(!_service.internal_start()) {
                    LOG_ERROR(_logger, "Couldn't start service {}", _implementationName);
                } else {
                    LOG_DEBUG(_logger, "Started {}", _implementationName);
                }
                return true;
            }

            return false;
        }

        template<class Interface0, class ...Interfaces>
        requires ImplementsDependencyInjection<ServiceType, Interface0>
        CPPELIX_CONSTEXPR void injectIntoSelf(uint64_t hashOfInterfaceToInject, const std::shared_ptr<ILifecycleManager> &dependentService) {
            if (typeNameHash<Interface0>() == hashOfInterfaceToInject) {
                _service.addDependencyInstance(static_cast<Interface0*>(dependentService->getInterfacePointer()));
            } else {
                if constexpr (sizeof...(Interfaces) > 0) {
                    injectIntoSelf<Interfaces...>(hashOfInterfaceToInject, dependentService);
                }
            }
        }

        CPPELIX_CONSTEXPR bool dependencyOffline(const std::shared_ptr<ILifecycleManager> &dependentService) final {
            auto dependency = dependentService->getSelfAsDependency();
            if(!_dependencies.contains(dependency) || !_satisfiedDependencies.contains(dependency)) {
                return false;
            }

            _satisfiedDependencies.removeDependency(dependency);



            bool stopped = true;
            if(_service.getState() != ServiceState::ACTIVE) {
                stopped = false;
            } else {
                bool shouldStop = _service.getState() == ServiceState::ACTIVE && !_dependencies.requiredDependenciesSatisfied(_satisfiedDependencies);

                if(shouldStop) {
                    if (!_service.internal_stop()) {
                        LOG_ERROR(_logger, "Couldn't stop service {}", _implementationName);
                        stopped = false;
                    } else {
                        LOG_DEBUG(_logger, "stopped {}", _implementationName);
                    }
                }
            }

            removeSelfInto<Dependencies...>(dependency.interfaceNameHash, dependentService);

            return stopped;
        }

        template<class Interface0, class ...Interfaces>
        requires ImplementsDependencyInjection<ServiceType, Interface0>
        CPPELIX_CONSTEXPR void removeSelfInto(uint64_t hashOfInterfaceToInject, const std::shared_ptr<ILifecycleManager> &dependentService) {
            if (typeNameHash<Interface0>() == hashOfInterfaceToInject) {
                _service.removeDependencyInstance(static_cast<Interface0*>(dependentService->getInterfacePointer()));
            } else {
                if constexpr (sizeof...(Interfaces) > 0) {
                    removeSelfInto<Interfaces...>(hashOfInterfaceToInject, dependentService);
                }
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

        [[nodiscard]] CPPELIX_CONSTEXPR Dependency getSelfAsDependency() const final {
            return Dependency{_nameHash, Interface::version, false};
        }

        [[nodiscard]] CPPELIX_CONSTEXPR DependencyInfo const * getDependencyInfo() const final {
            return &_dependencies;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR CppelixProperties const * getProperties() const final {
            return &_service._properties;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR void* getInterfacePointer() final {
            return &_service;
        }

    private:
        const std::string_view _implementationName;
        const uint64_t _nameHash;
        DependencyInfo _dependencies;
        DependencyInfo _satisfiedDependencies;
        ServiceType _service;
        IFrameworkLogger *_logger;
    };

    template<class Interface, class ServiceType>
    requires Derived<ServiceType, Service> && Derived<ServiceType, Interface>
    class LifecycleManager<Interface, ServiceType> final : public ILifecycleManager {
    public:
        explicit CPPELIX_CONSTEXPR LifecycleManager(IFrameworkLogger *logger, std::string_view name, uint64_t nameHash, CppelixProperties properties) : _implementationName(name), _nameHash(nameHash), _service(), _logger(logger) {
            _service.setProperties(std::move(properties));
        }

        CPPELIX_CONSTEXPR ~LifecycleManager() final = default;

        [[nodiscard]]
        static std::shared_ptr<LifecycleManager<Interface, ServiceType>> create(IFrameworkLogger *logger, std::string_view name, CppelixProperties properties) {
            if (name.empty()) {
                name = typeName<Interface>();
            }

            auto mgr = std::make_shared<LifecycleManager<Interface, ServiceType>>(logger, name, typeNameHash<Interface>(), std::move(properties));
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

        [[nodiscard]] CPPELIX_CONSTEXPR Dependency getSelfAsDependency() const final {
            return Dependency{_nameHash, Interface::version, false};
        }

        [[nodiscard]] CPPELIX_CONSTEXPR DependencyInfo const * getDependencyInfo() const final {
            return nullptr;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR CppelixProperties const * getProperties() const final {
            return &_service._properties;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR void* getInterfacePointer() final {
            return &_service;
        }

    private:
        const std::string_view _implementationName;
        const uint64_t _nameHash;
        ServiceType _service;
        IFrameworkLogger *_logger;
    };
}
