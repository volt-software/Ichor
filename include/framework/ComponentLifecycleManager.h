#pragma once

#include <string_view>
#include <memory>
#include <atomic>
#include <unordered_set>
#include "Bundle.h"
#include "interfaces/IFrameworkLogger.h"
#include "common.h"
#include <range/v3/view.hpp>

namespace Cppelix {
    enum class ComponentManagerState {
        INACTIVE,
        WAITING_FOR_REQUIRED,
        INSTANTIATED_AND_WAITING_FOR_REQUIRED,
        TRACKING_OPTIONAL
    };

    static std::atomic<uint64_t> _componentManagerIdCounter = 0;

    struct Dependency {
        constexpr Dependency(std::string_view _interfaceName, InterfaceVersion _interfaceVersion, bool _required) noexcept : interfaceName(_interfaceName), interfaceVersion(_interfaceVersion), required(_required) {}
        constexpr Dependency(const Dependency &other) noexcept = default;
        constexpr Dependency(Dependency &&other) noexcept = default;
        constexpr Dependency& operator=(const Dependency &other) noexcept = default;
        constexpr Dependency& operator=(Dependency &&other) noexcept = default;

        std::string_view interfaceName;
        InterfaceVersion interfaceVersion;
        bool required;
    };

    struct DependencyInfo {

        DependencyInfo() : _dependencies() {}

        template<class Interface>
        constexpr void addDependency(bool required = true) {
            _dependencies.emplace_back(typeName<Interface>(), Interface::version, required);
        }

        void addDependency(Dependency dependency) {
            _dependencies.emplace_back(dependency);
        }

        template<class Interface>
        constexpr void removeDependency() {
            std::erase(std::remove_if(begin(_dependencies), end(_dependencies), [](const auto& dep) noexcept { return dep.interfaceName == typeName<Interface>() && dep.interfaceVersion == Interface::version(); }), end(_dependencies));
        }

        void removeDependency(Dependency dependency) {
            _dependencies.erase(std::remove_if(begin(_dependencies), end(_dependencies), [dependency](const auto& dep) noexcept { return dep.interfaceName == dependency.interfaceName && dep.interfaceVersion == dependency.interfaceVersion; }), end(_dependencies));
        }

        template<class Interface>
        [[nodiscard]]
        constexpr bool contains() const {
            return cend(_dependencies) != std::find_if(cbegin(_dependencies), cend(_dependencies), [](const auto& dep) noexcept { return dep.interfaceName == typeName<Interface>() && dep.interfaceVersion == Interface::version(); });
        }

        [[nodiscard]]
        constexpr bool contains(Dependency dependency) const {
            return cend(_dependencies) != std::find_if(cbegin(_dependencies), cend(_dependencies), [&dependency](const auto& dep) noexcept { return dep.interfaceName == dependency.interfaceName && dep.interfaceVersion == dependency.interfaceVersion; });
        }

        [[nodiscard]]
        size_t size() const noexcept {
            return _dependencies.size();
        }

        [[nodiscard]]
        std::vector<Dependency> requiredDependencies() const {
            std::vector<Dependency> requiredDeps;
            std::copy_if(begin(_dependencies), end(_dependencies), std::back_inserter(requiredDeps), [](const auto& dep){ return dep.required; });
            return requiredDeps;
        }

        std::vector<Dependency> _dependencies;
    };

    class LifecycleManager {
    public:
        constexpr virtual ~LifecycleManager() = default;
        constexpr virtual void dependencyOnline(std::shared_ptr<LifecycleManager> dependentComponent) = 0;
        constexpr virtual void dependencyOffline(Dependency dependency) = 0;
        [[nodiscard]] constexpr virtual bool start() = 0;
        [[nodiscard]] constexpr virtual bool stop() = 0;
        [[nodiscard]] constexpr virtual bool shouldStart() = 0;
        [[nodiscard]] constexpr virtual std::string_view name() const = 0;
        [[nodiscard]] constexpr virtual ComponentManagerState getComponentManagerState() const = 0;
        [[nodiscard]] constexpr virtual uint64_t getComponentId() const = 0;
        [[nodiscard]] constexpr virtual Dependency getSelfAsDependency() const = 0;
        [[nodiscard]] constexpr virtual void* getComponentPointer() = 0;
    };

    template<class Interface, class ComponentType, typename... Dependencies>
    requires Derived<ComponentType, Bundle>
    class DependencyComponentLifecycleManager : public LifecycleManager {
    public:
        explicit constexpr DependencyComponentLifecycleManager(IFrameworkLogger *logger, std::string_view name) : _componentManagerId(_componentManagerIdCounter++), _name(name), _dependencies(), _satisfiedDependencies(), _component(), _logger(logger), _componentManagerState(ComponentManagerState::INACTIVE) {
            (_dependencies.addDependency<Dependencies>(), ...);
        }

        constexpr ~DependencyComponentLifecycleManager() final = default;

        [[nodiscard]]
        static std::shared_ptr<DependencyComponentLifecycleManager<Interface, ComponentType, Dependencies...>> create(IFrameworkLogger *logger, std::string_view name) {
            if (name.empty()) {
                if constexpr (std::is_same_v<Interface, IFrameworkLogger>) {
                    return std::make_shared<DependencyComponentLifecycleManager<Interface, ComponentType, Dependencies...>>(typeName<Interface>());
                }
                return std::make_shared<DependencyComponentLifecycleManager<Interface, ComponentType, Dependencies...>>(logger, typeName<Interface>());
            }


            if constexpr (std::is_same_v<Interface, IFrameworkLogger>) {
                return std::make_shared<DependencyComponentLifecycleManager<Interface, ComponentType, Dependencies...>>(name);
            }
            return std::make_shared<DependencyComponentLifecycleManager<Interface, ComponentType, Dependencies...>>(logger, name);
        }

        constexpr void dependencyOnline(std::shared_ptr<LifecycleManager> dependentComponent) final {
            std::scoped_lock l(_mutex);
            auto dependency = dependentComponent->getSelfAsDependency();
            if(!_dependencies.contains(dependency) || _satisfiedDependencies.contains(dependency)) {
                return;
            }

            _satisfiedDependencies.addDependency(dependency);
            injectSelfInto<Dependencies...>(dependency.interfaceName, dependentComponent);

            if(_dependencies.requiredDependencies().size() == _satisfiedDependencies.requiredDependencies().size()) {
                if(!_component.internal_start()) {
                    LOG_ERROR(_logger, "Couldn't start component {}", _name);
                }
            }
        };

        template<class Interface0, class ...Interfaces>
        constexpr void injectSelfInto(std::string_view nameOfInterfaceToInject, std::shared_ptr<LifecycleManager> dependentComponent) {
            if (typeName<Interface0>() == nameOfInterfaceToInject) {
                _component.addDependencyInstance(static_cast<Interface0*>(dependentComponent->getComponentPointer()));
            } else {
                if constexpr (sizeof...(Interfaces) > 0) {
                    injectSelfInto<Interfaces...>(nameOfInterfaceToInject, dependentComponent);
                }
            }
        }

        constexpr void dependencyOffline(Dependency dependency) final {
            std::scoped_lock l(_mutex);
            if(!_dependencies.contains(dependency) || !_satisfiedDependencies.contains(dependency)) {
                return;
            }

            _satisfiedDependencies.removeDependency(dependency);
            //_component.removeDependencyInstance(dependentComponent);

            if(_dependencies.requiredDependencies().size() != _satisfiedDependencies.requiredDependencies().size()) {
                if(!_component.internal_stop()) {
                    LOG_ERROR(_logger, "Couldn't stop component {}", _name);
                }
            }
        };

        [[nodiscard]]
        constexpr bool start() final {
            std::scoped_lock l(_mutex);
            if(_dependencies.size() == _satisfiedDependencies.size() && _component.getState() == BundleState::INSTALLED) {
                return _component.internal_start();
            }

            return false;
        }

        [[nodiscard]]
        constexpr bool stop() final {
            std::scoped_lock l(_mutex);
            if(_component.getState() == BundleState::ACTIVE) {
                return _component.internal_stop();
            }

            return false;
        }

        [[nodiscard]]
        constexpr bool shouldStart() final {
            std::scoped_lock l(_mutex);
            if(_component.getState() == BundleState::ACTIVE) {
                return false;
            }

            for(const auto& dep : _dependencies.requiredDependencies()) {
                if(!_satisfiedDependencies.contains(dep)) {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]]
        constexpr std::string_view name() const final {
            return _name;
        }

        [[nodiscard]]
        constexpr ComponentType& getComponent() {
            return _component;
        }

        [[nodiscard]]
        constexpr ComponentManagerState getComponentManagerState() const final {
            return _componentManagerState;
        }

        [[nodiscard]]
        constexpr uint64_t getComponentId() const final {
            return _componentManagerId;
        }

        [[nodiscard]] constexpr Dependency getSelfAsDependency() const final {
            return Dependency{_name, Interface::version, false};
        }

        [[nodiscard]] constexpr void* getComponentPointer() final {
            return &_component;
        }

    private:
        const uint64_t _componentManagerId;
        const std::string_view _name;
        DependencyInfo _dependencies;
        DependencyInfo _satisfiedDependencies;
        ComponentType _component;
        IFrameworkLogger *_logger;
        ComponentManagerState _componentManagerState;
        std::mutex _mutex;
    };

    template<class Interface, class ComponentType>
    requires Derived<ComponentType, Bundle>
    class ComponentLifecycleManager : public LifecycleManager {
    public:
        explicit constexpr ComponentLifecycleManager(std::string_view name) : _componentManagerId(_componentManagerIdCounter++), _name(name), _component(), _logger(&_component), _componentManagerState(ComponentManagerState::INACTIVE) {
            static_assert(std::is_same_v<Interface, IFrameworkLogger>, "Can only use this constructor if ComponentLifecycleManager is for IFrameworkLogger!");
        }

        explicit constexpr ComponentLifecycleManager(IFrameworkLogger *logger, std::string_view name) : _componentManagerId(_componentManagerIdCounter++), _name(name), _component(), _logger(logger), _componentManagerState(ComponentManagerState::INACTIVE) {}

        constexpr ~ComponentLifecycleManager() final = default;

        [[nodiscard]]
        static std::shared_ptr<ComponentLifecycleManager<Interface, ComponentType>> create(IFrameworkLogger *logger, std::string_view name) {
            if (name.empty()) {
                if constexpr (std::is_same_v<Interface, IFrameworkLogger>) {
                    return std::make_shared<ComponentLifecycleManager<Interface, ComponentType>>(typeName<Interface>());
                }
                return std::make_shared<ComponentLifecycleManager<Interface, ComponentType>>(logger, typeName<Interface>());
            }


            if constexpr (std::is_same_v<Interface, IFrameworkLogger>) {
                return std::make_shared<ComponentLifecycleManager<Interface, ComponentType>>(name);
            }
            return std::make_shared<ComponentLifecycleManager<Interface, ComponentType>>(logger, name);
        }

        constexpr void dependencyOnline(std::shared_ptr<LifecycleManager> dependentComponent) final {
        };

        constexpr void dependencyOffline(Dependency dependency) final {
        };

        [[nodiscard]]
        constexpr bool start() final {
            std::scoped_lock l(_mutex);
            if(_component.getState() == BundleState::INSTALLED) {
                return _component.internal_start();
            }

            return false;
        }

        [[nodiscard]]
        constexpr bool stop() final {
            std::scoped_lock l(_mutex);
            if(_component.getState() == BundleState::ACTIVE) {
                return _component.internal_stop();
            }

            return false;
        }

        [[nodiscard]]
        constexpr bool shouldStart() final {
            std::scoped_lock l(_mutex);
            if(_component.getState() == BundleState::ACTIVE) {
                return false;
            }

            return true;
        }

        [[nodiscard]]
        constexpr std::string_view name() const final {
            return _name;
        }

        [[nodiscard]]
        constexpr ComponentType& getComponent() {
            return _component;
        }

        [[nodiscard]]
        constexpr ComponentManagerState getComponentManagerState() const final {
            return _componentManagerState;
        }

        [[nodiscard]]
        constexpr uint64_t getComponentId() const final {
            return _componentManagerId;
        }

        [[nodiscard]] constexpr Dependency getSelfAsDependency() const final {
            return Dependency{_name, Interface::version, false};
        }

        [[nodiscard]] constexpr void* getComponentPointer() final {
            return &_component;
        }

    private:
        const uint64_t _componentManagerId;
        const std::string_view _name;
        ComponentType _component;
        IFrameworkLogger *_logger;
        ComponentManagerState _componentManagerState;
        std::mutex _mutex;
    };
}
