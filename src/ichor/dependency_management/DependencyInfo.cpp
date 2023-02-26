#include <ichor/dependency_management/DependencyInfo.h>
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
        _dependencies.emplace_back(dependency);
    }

    void DependencyInfo::removeDependency(const Dependency &dependency) {
        std::erase_if(_dependencies, [&dependency](auto const& dep) noexcept { return dep.interfaceNameHash == dependency.interfaceNameHash; });
    }

    [[nodiscard]]
    bool DependencyInfo::contains(const Dependency &dependency) const noexcept {
        return end() != std::find_if(begin(), end(), [&dependency](auto const& dep) noexcept { return dep.interfaceNameHash == dependency.interfaceNameHash; });
    }

    [[nodiscard]]
    std::vector<Dependency>::const_iterator DependencyInfo::find(const Dependency &dependency) const noexcept {
        return std::find_if(begin(), end(), [&dependency](auto const& dep) noexcept { return dep.interfaceNameHash == dependency.interfaceNameHash; });
    }

    [[nodiscard]]
    std::vector<Dependency>::iterator DependencyInfo::find(const Dependency &dependency) noexcept {
        return std::find_if(begin(), end(), [&dependency](auto const& dep) noexcept { return dep.interfaceNameHash == dependency.interfaceNameHash; });
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
        return std::all_of(begin(), end(), [](auto const &dep) {
            return dep.satisfied > 0 || !dep.required;
        });
    }
}
