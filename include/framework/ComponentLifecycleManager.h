#pragma once

#include <string_view>
#include <memory>
#include <atomic>
#include <unordered_set>
#include "Service.h"
#include "interfaces/IFrameworkLogger.h"
#include "Common.h"
#include "Filter.h"
#include "Property.h"
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
        CPPELIX_CONSTEXPR Dependency(std::string_view _interfaceName, InterfaceVersion _interfaceVersion, bool _required) noexcept : interfaceName(_interfaceName), interfaceVersion(_interfaceVersion), required(_required) {}
        CPPELIX_CONSTEXPR Dependency(const Dependency &other) noexcept = delete;
        CPPELIX_CONSTEXPR Dependency(Dependency &&other) noexcept = default;
        CPPELIX_CONSTEXPR Dependency& operator=(const Dependency &other) noexcept = delete;
        CPPELIX_CONSTEXPR Dependency& operator=(Dependency &&other) noexcept = default;
        CPPELIX_CONSTEXPR bool operator==(const Dependency &other) const noexcept {
            return interfaceName == other.interfaceName && interfaceVersion == other.interfaceVersion && required == other.required;
        }

        std::string_view interfaceName;
        InterfaceVersion interfaceVersion;
        bool required;
    };

    struct DependencyInfo {

        CPPELIX_CONSTEXPR DependencyInfo() : _dependencies() {}

        template<class Interface>
        CPPELIX_CONSTEXPR void addDependency(bool required = true) {
            _dependencies.emplace_back(typeName<Interface>(), Interface::version, required);
        }

        CPPELIX_CONSTEXPR void addDependency(Dependency dependency) {
            _dependencies.emplace_back(std::move(dependency));
        }

        template<class Interface>
        CPPELIX_CONSTEXPR void removeDependency() {
            std::erase(std::remove_if(begin(_dependencies), end(_dependencies), [](const auto& dep) noexcept { return dep.interfaceName == typeName<Interface>() && dep.interfaceVersion == Interface::version(); }), end(_dependencies));
        }

        CPPELIX_CONSTEXPR void removeDependency(const Dependency &dependency) {
            _dependencies.erase(std::remove_if(begin(_dependencies), end(_dependencies), [&dependency](const auto& dep) noexcept { return dep.interfaceName == dependency.interfaceName && dep.interfaceVersion == dependency.interfaceVersion; }), end(_dependencies));
        }

        template<class Interface>
        [[nodiscard]]
        CPPELIX_CONSTEXPR bool contains() const {
            return cend(_dependencies) != std::find_if(cbegin(_dependencies), cend(_dependencies), [](const auto& dep) noexcept { return dep.interfaceName == typeName<Interface>() && dep.interfaceVersion == Interface::version(); });
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool contains(const Dependency &dependency) const {
            return cend(_dependencies) != std::find_if(cbegin(_dependencies), cend(_dependencies), [&dependency](const auto& dep) noexcept { return dep.interfaceName == dependency.interfaceName && dep.interfaceVersion == dependency.interfaceVersion; });
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR size_t size() const noexcept {
            return _dependencies.size();
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR std::vector<const Dependency *> requiredDependencies() const {
            // bloody ranges doesn't deliver on what it promises.
            // using namespace ranges;
            //return _dependencies | view::filter([](const auto& dep){ return dep.required; }) | to<std::vector>;
            std::vector<const Dependency *> requiredDeps;
            for(auto &dep : _dependencies) {
                if(dep.required) {
                    requiredDeps.push_back(&dep);
                }
            }
            return requiredDeps;
        }

        CPPELIX_CONSTEXPR std::vector<Dependency> _dependencies;
    };

    class LifecycleManager {
    public:
        CPPELIX_CONSTEXPR virtual ~LifecycleManager() = default;
        CPPELIX_CONSTEXPR virtual void dependencyOnline(std::shared_ptr<LifecycleManager> dependentComponent) = 0;
        CPPELIX_CONSTEXPR virtual void dependencyOffline(std::shared_ptr<LifecycleManager> dependentComponent) = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual bool start() = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual bool stop() = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual bool shouldStart() = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual bool shouldStop() = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual std::string_view name() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual std::string_view type() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual uint64_t serviceId() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual ComponentManagerState getComponentManagerState() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual uint64_t getComponentId() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual Dependency getSelfAsDependency() const = 0;
        [[nodiscard]] CPPELIX_CONSTEXPR virtual void* getComponentPointer() = 0;

        template <typename T>
        [[nodiscard]] CPPELIX_CONSTEXPR T* getComponent() {
            return static_cast<T*>(getComponentPointer());
        }
    };

    template<class Interface, class ComponentType, typename... Dependencies>
    requires Derived<ComponentType, Service>
    class DependencyComponentLifecycleManager : public LifecycleManager {
    public:
        explicit CPPELIX_CONSTEXPR DependencyComponentLifecycleManager(IFrameworkLogger *logger, std::string_view name, CppelixProperties properties) : _componentManagerId(_componentManagerIdCounter++), _name(name), _properties(std::move(properties)), _dependencies(), _satisfiedDependencies(), _component(), _logger(logger), _componentManagerState(ComponentManagerState::INACTIVE) {
        }

        CPPELIX_CONSTEXPR ~DependencyComponentLifecycleManager() final = default;

        template<typename... Required, typename... Optional>
        [[nodiscard]]
        static std::shared_ptr<DependencyComponentLifecycleManager<Interface, ComponentType, Required..., Optional...>> create(IFrameworkLogger *logger, std::string_view name, CppelixProperties properties, RequiredList_t<Required...>, OptionalList_t<Optional...>) {
            if (name.empty()) {
                name = typeName<Interface>();
            }

            auto mgr = std::make_shared<DependencyComponentLifecycleManager<Interface, ComponentType, Required..., Optional...>>(logger, name, std::move(properties));
            mgr->template setupDependencies<Optional...>(false);
            mgr->template setupDependencies<Required...>(true);
            return mgr;
        }

        //stupid bug where it complains about accessing private variables.
        template <typename... TempDependencies>
        CPPELIX_CONSTEXPR void setupDependencies([[maybe_unused]] bool required) {
            (_dependencies.template addDependency<TempDependencies>(required), ...);
        }

        CPPELIX_CONSTEXPR void dependencyOnline(std::shared_ptr<LifecycleManager> dependentComponent) final {
            auto dependency = dependentComponent->getSelfAsDependency();
            if(!_dependencies.contains(dependency) || _satisfiedDependencies.contains(dependency)) {
                return;
            }

            injectSelfInto<Dependencies...>(dependency.interfaceName, dependentComponent);
            _satisfiedDependencies.addDependency(std::move(dependency));

            if(_dependencies.requiredDependencies().size() == _satisfiedDependencies.requiredDependencies().size()) {
                if(!_component.internal_start()) {
                    LOG_ERROR(_logger, "Couldn't start component {}", _name);
                }
            }
        }

        template<class Interface0, class ...Interfaces>
        CPPELIX_CONSTEXPR void injectSelfInto(std::string_view nameOfInterfaceToInject, std::shared_ptr<LifecycleManager> dependentComponent) {
            if (typeName<Interface0>() == nameOfInterfaceToInject) {
                _component.addDependencyInstance(static_cast<Interface0*>(dependentComponent->getComponentPointer()));
            } else {
                if constexpr (sizeof...(Interfaces) > 0) {
                    injectSelfInto<Interfaces...>(nameOfInterfaceToInject, dependentComponent);
                }
            }
        }

        CPPELIX_CONSTEXPR void dependencyOffline(std::shared_ptr<LifecycleManager> dependentComponent) final {
            auto dependency = dependentComponent->getSelfAsDependency();
            if(!_dependencies.contains(dependency) || !_satisfiedDependencies.contains(dependency)) {
                return;
            }

            removeSelfInto<Dependencies...>(dependency.interfaceName, dependentComponent);
            _satisfiedDependencies.removeDependency(dependency);

            if(_dependencies.requiredDependencies().size() != _satisfiedDependencies.requiredDependencies().size()) {
                if(!_component.internal_stop()) {
                    LOG_ERROR(_logger, "Couldn't stop component {}", _name);
                }
            }
        }

        template<class Interface0, class ...Interfaces>
        CPPELIX_CONSTEXPR void removeSelfInto(std::string_view nameOfInterfaceToInject, std::shared_ptr<LifecycleManager> dependentComponent) {
            if (typeName<Interface0>() == nameOfInterfaceToInject) {
                _component.removeDependencyInstance(static_cast<Interface0*>(dependentComponent->getComponentPointer()));
            } else {
                if constexpr (sizeof...(Interfaces) > 0) {
                    removeSelfInto<Interfaces...>(nameOfInterfaceToInject, dependentComponent);
                }
            }
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool start() final {
            if(_dependencies.requiredDependencies().size() == _satisfiedDependencies.size() && _component.getState() == ServiceState::INSTALLED) {
                return _component.internal_start();
            }

            return false;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool stop() final {
            if(_component.getState() == ServiceState::ACTIVE) {
                return _component.internal_stop();
            }

            return true;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool shouldStart() final {
            if(_component.getState() == ServiceState::ACTIVE) {
                return false;
            }

            for(const auto *dep : _dependencies.requiredDependencies()) {
                if(!_satisfiedDependencies.contains(*dep)) {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool shouldStop() final {
            if(_component.getState() != ServiceState::ACTIVE) {
                return false;
            }

            for(const auto *dep : _dependencies.requiredDependencies()) {
                if(!_satisfiedDependencies.contains(*dep)) {
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
        CPPELIX_CONSTEXPR std::string_view type() const final {
            return typeName<ComponentType>();
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR uint64_t serviceId() const final {
            return _component.get_service_id();
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR ComponentType& getComponent() {
            return _component;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR ComponentManagerState getComponentManagerState() const final {
            return _componentManagerState;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR uint64_t getComponentId() const final {
            return _componentManagerId;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR Dependency getSelfAsDependency() const final {
            return Dependency{_name, Interface::version, false};
        }

        [[nodiscard]] CPPELIX_CONSTEXPR void* getComponentPointer() final {
            return &_component;
        }

    private:
        const uint64_t _componentManagerId;
        const std::string_view _name;
        CppelixProperties _properties;
        DependencyInfo _dependencies;
        DependencyInfo _satisfiedDependencies;
        ComponentType _component;
        IFrameworkLogger *_logger;
        ComponentManagerState _componentManagerState;
    };

    template<class Interface, class ComponentType>
    requires Derived<ComponentType, Service>
    class ComponentLifecycleManager : public LifecycleManager {
    public:
        explicit CPPELIX_CONSTEXPR ComponentLifecycleManager(std::string_view name, CppelixProperties properties) : _componentManagerId(_componentManagerIdCounter++), _name(name), _properties(std::move(properties)), _component(), _logger(&_component), _componentManagerState(ComponentManagerState::INACTIVE) {
            static_assert(std::is_same_v<Interface, IFrameworkLogger>, "Can only use this constructor if ComponentLifecycleManager is for IFrameworkLogger!");
        }

        explicit CPPELIX_CONSTEXPR ComponentLifecycleManager(IFrameworkLogger *logger, std::string_view name, CppelixProperties properties) : _componentManagerId(_componentManagerIdCounter++), _name(name), _properties(std::move(properties)), _component(), _logger(logger), _componentManagerState(ComponentManagerState::INACTIVE) {}

        CPPELIX_CONSTEXPR ~ComponentLifecycleManager() final = default;

        [[nodiscard]]
        static std::shared_ptr<ComponentLifecycleManager<Interface, ComponentType>> create(IFrameworkLogger *logger, std::string_view name, CppelixProperties properties) {
            if (name.empty()) {
                name = typeName<Interface>();
            }


            if constexpr (std::is_same_v<Interface, IFrameworkLogger>) {
                return std::make_shared<ComponentLifecycleManager<Interface, ComponentType>>(name, std::move(properties));
            }
            return std::make_shared<ComponentLifecycleManager<Interface, ComponentType>>(logger, name, std::move(properties));
        }

        CPPELIX_CONSTEXPR void dependencyOnline(std::shared_ptr<LifecycleManager> dependentComponent) final {
        }

        CPPELIX_CONSTEXPR void dependencyOffline(std::shared_ptr<LifecycleManager> dependentComponent) final {
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool start() final {
            if(_component.getState() == ServiceState::INSTALLED) {
                return _component.internal_start();
            }

            return false;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool stop() final {
            if(_component.getState() == ServiceState::ACTIVE) {
                return _component.internal_stop();
            }

            return false;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR bool shouldStart() final {
            if(_component.getState() == ServiceState::ACTIVE) {
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
        CPPELIX_CONSTEXPR std::string_view type() const final {
            return typeName<ComponentType>();
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR uint64_t serviceId() const final {
            return _component.get_service_id();
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR ComponentType& getComponent() {
            return _component;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR ComponentManagerState getComponentManagerState() const final {
            return _componentManagerState;
        }

        [[nodiscard]]
        CPPELIX_CONSTEXPR uint64_t getComponentId() const final {
            return _componentManagerId;
        }

        [[nodiscard]] CPPELIX_CONSTEXPR Dependency getSelfAsDependency() const final {
            return Dependency{_name, Interface::version, false};
        }

        [[nodiscard]] CPPELIX_CONSTEXPR void* getComponentPointer() final {
            return &_component;
        }

    private:
        const uint64_t _componentManagerId;
        const std::string_view _name;
        CppelixProperties _properties;
        ComponentType _component;
        IFrameworkLogger *_logger;
        ComponentManagerState _componentManagerState;
    };
}
