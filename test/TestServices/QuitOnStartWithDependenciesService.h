#pragma once

#include "UselessService.h"

using namespace Ichor;

struct QuitOnStartWithDependenciesService final : public AdvancedService<QuitOnStartWithDependenciesService> {
    QuitOnStartWithDependenciesService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<IUselessService>(this, true);
    }
    ~QuitOnStartWithDependenciesService() final = default;
    Task<tl::expected<void, Ichor::StartError>> start() final {
        GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());
        co_return {};
    }

    void addDependencyInstance(IUselessService *, IService *) {
    }

    void removeDependencyInstance(IUselessService *, IService *) {
    }
};
