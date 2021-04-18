#pragma once

#include <ichor/Dependency.h>

namespace Ichor {
    struct DependencyInfo final {

        ICHOR_CONSTEXPR explicit DependencyInfo(std::pmr::memory_resource *memResource) : _dependencies(memResource) {
        }

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

        ICHOR_CONSTEXPR std::pmr::vector<Dependency> _dependencies;
    };
}