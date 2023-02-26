#pragma once

#include "Dependency.h"
#include <vector>

namespace Ichor {
    struct DependencyInfo final {

        explicit DependencyInfo() = default;

        ~DependencyInfo() = default;
        DependencyInfo(const DependencyInfo&) = delete;
        DependencyInfo(DependencyInfo&&) noexcept = default;
        DependencyInfo& operator=(const DependencyInfo&) = delete;
        DependencyInfo& operator=(DependencyInfo&&) noexcept = default;

        [[nodiscard]]
        std::vector<Dependency>::iterator begin();

        [[nodiscard]]
        std::vector<Dependency>::const_iterator begin() const noexcept;

        [[nodiscard]]
        std::vector<Dependency>::iterator end();

        [[nodiscard]]
        std::vector<Dependency>::const_iterator end() const noexcept;

        void addDependency(Dependency dependency);

        void removeDependency(const Dependency &dependency);

        [[nodiscard]]
        bool contains(const Dependency &dependency) const noexcept;

        [[nodiscard]]
        std::vector<Dependency>::const_iterator find(const Dependency &dependency) const noexcept;

        [[nodiscard]]
        std::vector<Dependency>::iterator find(const Dependency &dependency) noexcept;

        [[nodiscard]]
        size_t size() const noexcept;

        [[nodiscard]]
        bool empty() const noexcept;

        [[nodiscard]]
        bool allSatisfied() const noexcept;

        std::vector<Dependency> _dependencies;
    };
}
