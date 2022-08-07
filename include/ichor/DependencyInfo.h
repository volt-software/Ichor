#pragma once

#include <ichor/Dependency.h>

namespace Ichor {
    struct DependencyInfo final {

        ICHOR_CONSTEXPR explicit DependencyInfo() = default;

        ICHOR_CONSTEXPR ~DependencyInfo() = default;
        ICHOR_CONSTEXPR DependencyInfo(const DependencyInfo&) = delete;
        ICHOR_CONSTEXPR DependencyInfo(DependencyInfo&&) noexcept = default;
        ICHOR_CONSTEXPR DependencyInfo& operator=(const DependencyInfo&) = delete;
        ICHOR_CONSTEXPR DependencyInfo& operator=(DependencyInfo&&) noexcept = default;

        [[nodiscard]]
        ICHOR_CONSTEXPR auto begin() {
            return std::begin(_dependencies);
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR auto begin() const noexcept {
            return std::cbegin(_dependencies);
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR auto end() {
            return std::end(_dependencies);
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR auto end() const noexcept {
            return std::cend(_dependencies);
        }

        template<class Interface>
        ICHOR_CONSTEXPR void addDependency(bool required = true) {
            _dependencies.emplace_back(typeNameHash<Interface>(), required, false);
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
        ICHOR_CONSTEXPR bool contains() const noexcept {
            return end() != std::find_if(begin(), end(), [](auto const& dep) noexcept { return dep.interfaceNameHash == typeNameHash<Interface>(); });
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR bool contains(const Dependency &dependency) const noexcept {
            return end() != std::find_if(begin(), end(), [&dependency](auto const& dep) noexcept { return dep.interfaceNameHash == dependency.interfaceNameHash; });
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR auto find(const Dependency &dependency) const noexcept {
            return std::find_if(begin(), end(), [&dependency](auto const& dep) noexcept { return dep.interfaceNameHash == dependency.interfaceNameHash; });
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR auto find(const Dependency &dependency) noexcept {
            return std::find_if(begin(), end(), [&dependency](auto const& dep) noexcept { return dep.interfaceNameHash == dependency.interfaceNameHash; });
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR size_t size() const noexcept {
            return _dependencies.size();
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR bool empty() const noexcept {
            return _dependencies.empty();
        }

        [[nodiscard]]
        ICHOR_CONSTEXPR bool allSatisfied() const noexcept {
            return std::all_of(begin(), end(), [](auto const &dep) {
                return dep.satisfied > 0 || !dep.required;
            });
        }

        ICHOR_CONSTEXPR std::vector<Dependency> _dependencies;
    };
}