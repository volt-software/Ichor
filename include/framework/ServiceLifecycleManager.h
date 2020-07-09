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

    class LifecycleManager {
    public:
        CPPELIX_CONSTEXPR virtual ~LifecycleManager() = default;
        ///
        /// \param dependentService
        /// \return true if started, false if not
        CPPELIX_CONSTEXPR virtual bool dependencyOnline(const std::shared_ptr<LifecycleManager> &dependentService) = 0;
        ///
        /// \param dependentService
        /// \return true if stopped, false if not
        CPPELIX_CONSTEXPR virtual bool dependencyOffline(const std::shared_ptr<LifecycleManager> &dependentService) = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual bool start() = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual bool stop() = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual std::string_view name() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual uint64_t type() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual uint64_t serviceId() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual ServiceState getServiceState() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual Dependency getSelfAsDependency() const = 0;

        // for some reason, returning a reference produces garbage??
        [[nodiscard]] CPPELIX_CONSTEXPR virtual DependencyInfo const * getDependencyInfo() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual CppelixProperties const * getProperties() const = 0;
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
        explicit CPPELIX_CONSTEXPR DependencyServiceLifecycleManager(IFrameworkLogger *logger, std::string_view name, uint64_t nameHash, CppelixProperties properties) : _name(name), _nameHash(nameHash), _dependencies(), _satisfiedDependencies(), _service(), _logger(logger) {
            _service.setProperties(std::move(properties));
        }

        CPPELIX_CONSTEXPR ~DependencyServiceLifecycleManager() final {
            // _manager is always injected in DependencyManager::create...Manager functions.
            (_service._manager->template pushEvent<DependencyUndoRequestEvent>(_service.getServiceId(), nullptr, Dependency{typeNameHash<Dependencies>(), Dependencies::version, false}, getProperties()), ...);
        }

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

        // stupid bug where it complains about accessing private variables.
        template <typename... TempDependencies>
        CPPELIX_CONSTEXPR void setupDependencies([[maybe_unused]] bool required) {
            (_dependencies.template addDependency<TempDependencies>(required), ...);
        }

        CPPELIX_CONSTEXPR bool dependencyOnline(const std::shared_ptr<LifecycleManager> &dependentService) final {
            auto dependency = dependentService->getSelfAsDependency();
            if(!_dependencies.contains(dependency) || _satisfiedDependencies.contains(dependency)) {
                return false;
            }

            injectIntoSelf<Dependencies...>(dependency.interfaceNameHash, dependentService);
            _satisfiedDependencies.addDependency(std::move(dependency));

            bool canStart = _service.getState() != ServiceState::ACTIVE && _dependencies.requiredDependenciesSatisfied(_satisfiedDependencies);
            if(canStart) {
                if(!_service.internal_start()) {
                    LOG_ERROR(_logger, "Couldn't start service {}", _name);
                } else {
                    LOG_DEBUG(_logger, "Started {}", _name);
                }
                return true;
            }

            return false;
        }

        template<class Interface0, class ...Interfaces>
        requires ImplementsDependencyInjection<ServiceType, Interface0>
        CPPELIX_CONSTEXPR void injectIntoSelf(uint64_t hashOfInterfaceToInject, const std::shared_ptr<LifecycleManager> &dependentService) {
            if (typeNameHash<Interface0>() == hashOfInterfaceToInject) {
                _service.addDependencyInstance(static_cast<Interface0*>(dependentService->getServicePointer()));
            } else {
                if constexpr (sizeof...(Interfaces) > 0) {
                    injectIntoSelf<Interfaces...>(hashOfInterfaceToInject, dependentService);
                }
            }
        }

        CPPELIX_CONSTEXPR bool dependencyOffline(const std::shared_ptr<LifecycleManager> &dependentService) final {
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
                        LOG_ERROR(_logger, "Couldn't stop service {}", _name);
                        stopped = false;
                    } else {
                        LOG_DEBUG(_logger, "stopped {}", _name);
                    }
                }
            }

            removeSelfInto<Dependencies...>(dependency.interfaceNameHash, dependentService);

            return stopped;
        }

        template<class Interface0, class ...Interfaces>
        requires ImplementsDependencyInjection<ServiceType, Interface0>
        CPPELIX_CONSTEXPR void removeSelfInto(uint64_t hashOfInterfaceToInject, const std::shared_ptr<LifecycleManager> &dependentService) {
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
            bool canStart = _service.getState() != ServiceState::ACTIVE && _dependencies.requiredDependenciesSatisfied(_satisfiedDependencies);
            if (canStart) {
                if(_service.internal_start()) {
                    LOG_DEBUG(_logger, "Started {}", _name);
                    return true;
                } else {
                    LOG_DEBUG(_logger, "Couldn't start {}", _name);
                }
            }

            return false;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool stop() final {
            if(_service.getState() == ServiceState::ACTIVE) {
                if(_service.internal_stop()) {
                    LOG_DEBUG(_logger, "Stopped {}", _name);
                    return true;
                } else {
                    LOG_DEBUG(_logger, "Couldn't stop {}", _name);
                }
            }

            return true;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR std::string_view name() const final {
            return _name;
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

        [[nodiscard]] CPPELIX_CONSTEXPR void* getServicePointer() final {
            return &_service;
        }

    private:
        const std::string_view _name;
        const uint64_t _nameHash;
        DependencyInfo _dependencies;
        DependencyInfo _satisfiedDependencies;
        ServiceType _service;
        IFrameworkLogger *_logger;
    };

    template<class Interface, class ServiceType>
    requires Derived<ServiceType, Service>
    class ServiceLifecycleManager : public LifecycleManager {
    public:
        explicit CPPELIX_CONSTEXPR ServiceLifecycleManager(std::string_view name, uint64_t nameHash, CppelixProperties properties) : _name(name), _nameHash(nameHash), _service(), _logger(&_service) {
            static_assert(std::is_same_v<Interface, IFrameworkLogger>, "Can only use this constructor if ServiceLifecycleManager is for IFrameworkLogger!");
            _service.setProperties(std::move(properties));
        }

        explicit CPPELIX_CONSTEXPR ServiceLifecycleManager(IFrameworkLogger *logger, std::string_view name, uint64_t nameHash, CppelixProperties properties) : _name(name), _nameHash(nameHash), _service(), _logger(logger) {
            _service.setProperties(std::move(properties));
        }

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

        CPPELIX_CONSTEXPR bool dependencyOnline(const std::shared_ptr<LifecycleManager> &dependentService) final {
            return false;
        }

        CPPELIX_CONSTEXPR bool dependencyOffline(const std::shared_ptr<LifecycleManager> &dependentService) final {
            return false;
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

        [[nodiscard]] CPPELIX_CONSTEXPR std::string_view name() const final {
            return _name;
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
            return Dependency{typeNameHash<Interface>(), Interface::version, false};
        }

        [[nodiscard]] CPPELIX_CONSTEXPR DependencyInfo const * getDependencyInfo() const final {
            return nullptr;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR CppelixProperties const * getProperties() const final {
            return &_service._properties;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR void* getServicePointer() final {
            return &_service;
        }

    private:
        const std::string_view _name;
        uint64_t _nameHash;
        ServiceType _service;
        IFrameworkLogger *_logger;
    };
}
