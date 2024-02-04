#pragma once

#include "UselessService.h"
#include <ichor/dependency_management/DependencyRegister.h>

using namespace Ichor;

struct RegistrationCheckerService final : public AdvancedService<RegistrationCheckerService> {
    RegistrationCheckerService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<IUselessService>(this, DependencyFlags::NONE);
        reg.registerDependency<Ichor::IUselessService>(this, DependencyFlags::NONE);
    }
    ~RegistrationCheckerService() final = default;

    Task<tl::expected<void, Ichor::StartError>> start() final {
        if(_depCount == 2) {
            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());
        }
        co_return {};
    }

    void addDependencyInstance(IUselessService&, IService&) {
        _depCount++;

        fmt::print("registrations requests: {}\n", _depCount);

        if(_depCount == 2) {
            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());
        }
    }

    void removeDependencyInstance(IUselessService&, IService&) {
    }

    uint64_t _depCount{};
};
