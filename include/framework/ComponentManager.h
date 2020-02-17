#pragma once

#include <string_view>
#include <memory>
#include <unordered_set>
#include "Bundle.h"
#include "interfaces/IFrameworkLogger.h"

namespace Cppelix {
    constexpr bool isUpper(char C) { return (C >= 'A' && C <= 'Z'); }
    constexpr bool isLower(char C) { return (C >= 'a' && C <= 'z'); }
    constexpr bool isDigit(char c) { return (c >= '0' && c <= '9'); }
    constexpr bool isAlpha(char c) { return isUpper(c) || isLower(c); }

    constexpr bool isAlphanumeric(char c) {
        return isAlpha(c) || isDigit(c);
    }

    template<typename INTERFACE_TYPENAME>
    [[nodiscard]] constexpr auto typeName() {
        constexpr std::string_view result = __PRETTY_FUNCTION__;
        constexpr std::string_view templateStr = "INTERFACE_TYPENAME = ";

        constexpr size_t bpos = result.find(templateStr) + templateStr.size(); //find begin pos after INTERFACE_TYPENAME = entry
        if constexpr (result.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_:") == std::string_view::npos) {
            constexpr size_t len = result.length() - bpos;

            static_assert(!result.substr(bpos, len).empty(), "Cannot infer type name in function call");

            return result.substr(bpos, len);
        } else {
            constexpr size_t len = result.substr(bpos).find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_:");

            static_assert(!result.substr(bpos, len).empty(), "Cannot infer type name in function call");

            return result.substr(bpos, len);
        }
    }

    class LifecycleManager {
    public:
        virtual ~LifecycleManager() = default;
        virtual void dependencyOnline(std::string_view name) = 0;
        virtual void dependencyOffline(std::string_view name) = 0;
        virtual bool start() = 0;
        virtual bool stop() = 0;
        virtual std::string_view name() = 0;
    };

    template<class Interface, class ComponentType>
    requires Derived<ComponentType, Bundle>
    class ComponentManager : public LifecycleManager {
    public:
        explicit constexpr ComponentManager(std::string_view name) : _name(name), _dependencies(), _satisfiedDependencies(), _component(), _logger(&_component) {
            static_assert(std::is_same_v<Interface, IFrameworkLogger>, "Can only use this constructor if ComponentManager is for IFrameworkLogger!");
        }

        explicit constexpr ComponentManager(IFrameworkLogger *logger, std::string_view name) : _name(name), _dependencies(), _satisfiedDependencies(), _component(), _logger(logger) {}

        static constexpr ComponentManager<Interface, ComponentType> create(std::string_view name) {
            if (name.empty()) {
                return ComponentManager<Interface, ComponentType>(typeName<ComponentType>());
            }

            return ComponentManager<Interface, ComponentType>(name);
        }

        static constexpr ComponentManager<Interface, ComponentType> create(IFrameworkLogger *logger, std::string_view name) {
            if (name.empty()) {
                return ComponentManager<Interface, ComponentType>(logger, typeName<ComponentType>());
            }

            return ComponentManager<Interface, ComponentType>(logger, name);
        }

        template<class dependentInterface>
        constexpr ComponentManager<Interface, ComponentType> addDependency() {
            LOG_TRACE(_logger, "Adding dependency {} to component {}", typeName<dependentInterface>(), _component.name());
            _dependencies.emplace(typeName<dependentInterface>());
            return this;
        }

        constexpr void dependencyOnline(std::string_view name) final {
            if(!_dependencies.contains(name) || _satisfiedDependencies.contains(name)) {
                return;
            }

            _satisfiedDependencies.emplace(name);

            if(_dependencies.size() == _satisfiedDependencies.size()) {
                if(!_component.internal_start()) {
                    LOG_ERROR(_logger, "Couldn't start component {}", _name);
                }
            }
        };

        constexpr void dependencyOffline(std::string_view name) final {
            if(!_dependencies.contains(name) || !_satisfiedDependencies.contains(name)) {
                return;
            }

            _satisfiedDependencies.erase(name);

            if(_dependencies.size() != _satisfiedDependencies.size()) {
                if(!_component.internal_stop()) {
                    LOG_ERROR(_logger, "Couldn't stop component {}", _name);
                }
            }
        };

        constexpr bool start() final {
            if(_dependencies.size() == _satisfiedDependencies.size() && _component.getState() == BundleState::INSTALLED) {
                return _component.internal_start();
            }

            return false;
        }

        constexpr bool stop() final {
            if(_component.getState() == BundleState::ACTIVE) {
                return _component.internal_stop();
            }

            return false;
        }

        [[nodiscard]]
        constexpr std::string_view name() final {
            return _name;
        }

        [[nodiscard]]
        constexpr ComponentType& getComponent() {
            return _component;
        }

    private:
        const std::string_view _name;
        std::unordered_set<std::string_view> _dependencies;
        std::unordered_set<std::string_view> _satisfiedDependencies;
        ComponentType _component;
        IFrameworkLogger *_logger;
    };
}
