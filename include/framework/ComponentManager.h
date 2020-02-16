#pragma once

#include <string_view>
#include <memory>
#include <unordered_set>
#include <spdlog/spdlog.h>
#include "Bundle.h"

namespace Cppelix {
    template <class T, class U>
    concept Derived = std::is_base_of<U, T>::value;

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

            if (result.substr(bpos, len).empty()) {
                SPDLOG_ERROR("Cannot infer type name in function call '{}'", __PRETTY_FUNCTION__);
            }

            return result.substr(bpos, len);
        } else {
            constexpr size_t len = result.substr(bpos).find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_:");

            if (result.substr(bpos, len).empty()) {
                SPDLOG_ERROR("Cannot infer type name in function call '{}'", __PRETTY_FUNCTION__);
            }

            return result.substr(bpos, len);
        }
    }

    class LifecycleManager {
    public:
        virtual ~LifecycleManager() = default;
        constexpr virtual bool start() = 0;
        constexpr virtual bool stop() = 0;
        constexpr virtual void dependencyOnline(std::string_view name) = 0;
        constexpr virtual void dependencyOffline(std::string_view name) = 0;
        constexpr virtual std::string_view name() = 0;
    };

    template<class Interface, class Impl>
    requires Derived<Impl, Bundle>
    class ComponentManager : public LifecycleManager {
    public:
        explicit constexpr ComponentManager(std::string_view name) : _name(name), _dependencies(), _satisfiedDependencies() {}

        static constexpr ComponentManager<Interface, Impl> create(std::string_view name) {
            if (name.empty()) {
                return ComponentManager<Interface, Impl>(typeName<Impl>());
            }

            return ComponentManager<Interface, Impl>(name);
        }

        template<class dependentInterface>
        constexpr ComponentManager<Interface, Impl> addDependency() {
            _dependencies.emplace(typeName<dependentInterface>());
            return this;
        }

        [[nodiscard]]
        constexpr bool start() final {
            SPDLOG_DEBUG("{}", _name);
            return true;
        };

        [[nodiscard]]
        constexpr bool stop() final {
            SPDLOG_DEBUG("{}", _name);
            return true;
        };

        constexpr void dependencyOnline(std::string_view name) final {
            SPDLOG_DEBUG("{}", _name);
            if(_dependencies.contains(name)) {
                _satisfiedDependencies.emplace(name);
            }
        };

        constexpr void dependencyOffline(std::string_view name) final {
            SPDLOG_DEBUG("{}", _name);
            if(_dependencies.contains(name)) {
                _satisfiedDependencies.erase(name);
            }
        };

        [[nodiscard]]
        constexpr std::string_view name() final {
            return _name;
        }

    private:
        const std::string_view _name;
        std::unordered_set<std::string_view> _dependencies;
        std::unordered_set<std::string_view> _satisfiedDependencies;
    };
}
