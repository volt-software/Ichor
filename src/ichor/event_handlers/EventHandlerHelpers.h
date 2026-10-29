#pragma once

#include <algorithm>
#include <ichor/dependency_management/ILifecycleManager.h>
#include <ichor/dependency_management/DependencyRegister.h>

namespace Ichor::v1::Detail {
    // Whether dependent requests one of the interfaces provider offers, i.e. whether a filter rejecting the pair is worth an internal debug line
    [[nodiscard]] inline bool requestsAnyInterfaceOf(Ichor::ILifecycleManager const &dependent, Ichor::ILifecycleManager const &provider) noexcept {
        auto const *registry = dependent.getDependencyRegistry();
        return registry != nullptr && std::ranges::any_of(provider.getInterfaces(), [registry](Ichor::Dependency const &dep) noexcept { return registry->contains(dep); });
    }
}
