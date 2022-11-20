#pragma once

#include "Dependency.h"
#include <algorithm>

namespace Ichor {
    struct DependencyInfo final {

        explicit DependencyInfo() = default;

        ~DependencyInfo() = default;
        DependencyInfo(const DependencyInfo&) = delete;
        DependencyInfo(DependencyInfo&&) noexcept = default;
        DependencyInfo& operator=(const DependencyInfo&) = delete;
        DependencyInfo& operator=(DependencyInfo&&) noexcept = default;

        [[nodiscard]]
        auto begin() {
            return std::begin(_dependencies);
        }

        [[nodiscard]]
        auto begin() const noexcept {
            return std::cbegin(_dependencies);
        }

        [[nodiscard]]
        auto end() {
            return std::end(_dependencies);
        }

        [[nodiscard]]
        auto end() const noexcept {
            return std::cend(_dependencies);
        }

        template<class Interface>
        void addDependency(bool required = true) {
            _dependencies.emplace_back(typeNameHash<Interface>(), required, false);
        }

        void addDependency(Dependency dependency) {
            _dependencies.emplace_back(dependency);
        }

        template<class Interface>
        void removeDependency() {
            std::erase_if(_dependencies, [](auto const& dep) noexcept { return dep.interfaceNameHash == typeNameHash<Interface>(); });
        }

        void removeDependency(const Dependency &dependency) {
            std::erase_if(_dependencies, [&dependency](auto const& dep) noexcept { return dep.interfaceNameHash == dependency.interfaceNameHash; });
        }

        template<class Interface>
        [[nodiscard]]
        bool contains() const noexcept {
            return end() != std::find_if(begin(), end(), [](auto const& dep) noexcept { return dep.interfaceNameHash == typeNameHash<Interface>(); });
        }

        [[nodiscard]]
        bool contains(const Dependency &dependency) const noexcept {
            return end() != std::find_if(begin(), end(), [&dependency](auto const& dep) noexcept { return dep.interfaceNameHash == dependency.interfaceNameHash; });
        }

        [[nodiscard]]
        auto find(const Dependency &dependency) const noexcept {
            return std::find_if(begin(), end(), [&dependency](auto const& dep) noexcept { return dep.interfaceNameHash == dependency.interfaceNameHash; });
        }

        [[nodiscard]]
        auto find(const Dependency &dependency) noexcept {
            return std::find_if(begin(), end(), [&dependency](auto const& dep) noexcept { return dep.interfaceNameHash == dependency.interfaceNameHash; });
        }

        [[nodiscard]]
        size_t size() const noexcept {
            return _dependencies.size();
        }

        [[nodiscard]]
        bool empty() const noexcept {
            return _dependencies.empty();
        }

        [[nodiscard]]
        bool allSatisfied() const noexcept {
            return std::all_of(begin(), end(), [](auto const &dep) {
                return dep.satisfied > 0 || !dep.required;
            });
        }

        std::vector<Dependency> _dependencies;
    };
}