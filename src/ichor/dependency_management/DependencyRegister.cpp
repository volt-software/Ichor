#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/Common.h>
#include <algorithm>

namespace Ichor {

    [[nodiscard]]
    auto DependencyRegister::begin() -> typename decltype(_registrations)::iterator {
        return std::begin(_registrations);
    }

    [[nodiscard]]
    auto DependencyRegister::begin() const noexcept -> typename decltype(_registrations)::const_iterator {
        return std::cbegin(_registrations);
    }

    [[nodiscard]]
    auto DependencyRegister::end() -> typename decltype(_registrations)::iterator {
        return std::end(_registrations);
    }

    [[nodiscard]]
    auto DependencyRegister::end() const noexcept -> typename decltype(_registrations)::const_iterator {
        return std::cend(_registrations);
    }

    using PairType = std::pair<uint64_t, const std::tuple<Dependency, std::function<void(NeverNull<void*>, IService&)>, std::function<void(NeverNull<void*>, IService&)>, tl::optional<Properties>>>;

    [[nodiscard]]
    bool DependencyRegister::contains(const Dependency &dependency) const noexcept {
        return end() != std::find_if(begin(), end(), [&dependency](PairType const& dep) noexcept {
            return std::get<Dependency>(dep.second).interfaceNameHash == dependency.interfaceNameHash;
        });
    }

    [[nodiscard]]
    auto DependencyRegister::find(const Dependency &dependency, bool satisfied) const noexcept -> typename decltype(_registrations)::const_iterator  {
        INTERNAL_DEBUG("const find() size {}", size());
        return std::find_if(begin(), end(), [&dependency, satisfied](PairType const& node) noexcept {
            auto const &dep = std::get<Dependency>(node.second);
            INTERNAL_DEBUG("const find() {}:{}, {}:{}", dep.interfaceNameHash, dependency.interfaceNameHash, dep.satisfied, satisfied);
            return dep.interfaceNameHash == dependency.interfaceNameHash && ((dep.flags & DependencyFlags::ALLOW_MULTIPLE) == DependencyFlags::ALLOW_MULTIPLE || (satisfied && dep.satisfied > 0) || (!satisfied && dep.satisfied == 0));
        });
    }

    [[nodiscard]]
    auto DependencyRegister::find(const Dependency &dependency, bool satisfied) noexcept -> typename decltype(_registrations)::iterator  {
        INTERNAL_DEBUG("find() size {}", size());
        return std::find_if(begin(), end(), [&dependency, satisfied](PairType const& node) noexcept {
            auto const &dep = std::get<Dependency>(node.second);
            INTERNAL_DEBUG("find() {}:{}, {}:{}", dep.interfaceNameHash, dependency.interfaceNameHash, dep.satisfied, satisfied);
            return dep.interfaceNameHash == dependency.interfaceNameHash && ((dep.flags & DependencyFlags::ALLOW_MULTIPLE) == DependencyFlags::ALLOW_MULTIPLE || (satisfied && dep.satisfied > 0) || (!satisfied && dep.satisfied == 0));
        });
    }

    [[nodiscard]]
    size_t DependencyRegister::size() const noexcept {
        return _registrations.size();
    }

    [[nodiscard]]
    bool DependencyRegister::empty() const noexcept {
        return _registrations.empty();
    }

    [[nodiscard]]
    bool DependencyRegister::allSatisfied() const noexcept {
        return std::all_of(begin(), end(), [](PairType const &node) noexcept {
            auto const &dep = std::get<Dependency>(node.second);
            return dep.satisfied > 0 || (dep.flags & DependencyFlags::REQUIRED) == 0;
        });
    }
}
