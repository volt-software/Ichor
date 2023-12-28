#include <ichor/dependency_management/DependencyInfo.h>
#include <ichor/Common.h>
#include <algorithm>

namespace Ichor {

    [[nodiscard]]
    std::vector<Dependency>::iterator DependencyInfo::begin() {
        return std::begin(_dependencies);
    }

    [[nodiscard]]
    std::vector<Dependency>::const_iterator DependencyInfo::begin() const noexcept {
        return std::cbegin(_dependencies);
    }

    [[nodiscard]]
    std::vector<Dependency>::iterator DependencyInfo::end() {
        return std::end(_dependencies);
    }

    [[nodiscard]]
    std::vector<Dependency>::const_iterator DependencyInfo::end() const noexcept {
        return std::cend(_dependencies);
    }

    void DependencyInfo::addDependency(Dependency dependency) {
        _dependencies.emplace_back(std::move(dependency));
    }

    [[nodiscard]]
    bool DependencyInfo::contains(const Dependency &dependency) const noexcept {
        return end() != std::find_if(begin(), end(), [&dependency](auto const& dep) noexcept { return dep.interfaceNameHash == dependency.interfaceNameHash; });
    }

    [[nodiscard]]
    std::vector<Dependency>::const_iterator DependencyInfo::find(const Dependency &dependency, bool satisfied) const noexcept {
        INTERNAL_DEBUG("const find() size {}", size());
        return std::find_if(begin(), end(), [&dependency, satisfied](Dependency const& dep) noexcept {
            INTERNAL_DEBUG("const find() {}:{}, {}:{}", dep.interfaceNameHash, dependency.interfaceNameHash, dep.satisfied, satisfied);
            return dep.interfaceNameHash == dependency.interfaceNameHash && ((dep.flags & DependencyFlags::ALLOW_MULTIPLE) == DependencyFlags::ALLOW_MULTIPLE || (satisfied && dep.satisfied > 0) || (!satisfied && dep.satisfied == 0));
        });
    }

    [[nodiscard]]
    std::vector<Dependency>::iterator DependencyInfo::find(const Dependency &dependency, bool satisfied) noexcept {
        INTERNAL_DEBUG("find() size {}", size());
        return std::find_if(begin(), end(), [&dependency, satisfied](Dependency const& dep) noexcept {
            INTERNAL_DEBUG("find() {}:{}, {}:{}", dep.interfaceNameHash, dependency.interfaceNameHash, dep.satisfied, satisfied);
            return dep.interfaceNameHash == dependency.interfaceNameHash && ((dep.flags & DependencyFlags::ALLOW_MULTIPLE) == DependencyFlags::ALLOW_MULTIPLE || (satisfied && dep.satisfied > 0) || (!satisfied && dep.satisfied == 0));
        });
    }

    [[nodiscard]]
    size_t DependencyInfo::size() const noexcept {
        return _dependencies.size();
    }

    [[nodiscard]]
    bool DependencyInfo::empty() const noexcept {
        return _dependencies.empty();
    }

    [[nodiscard]]
    bool DependencyInfo::allSatisfied() const noexcept {
        return std::all_of(begin(), end(), [](auto const &dep) noexcept {
            return dep.satisfied > 0 || (dep.flags & DependencyFlags::REQUIRED) == 0;
        });
    }
}
