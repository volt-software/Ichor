#pragma once

#include "UselessService.h"

using namespace Ichor;

struct QuitOnStartWithDependenciesService final : public Service<QuitOnStartWithDependenciesService> {
    QuitOnStartWithDependenciesService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<IUselessService>(this, true);
    }
    ~QuitOnStartWithDependenciesService() final = default;
    AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
        getManager().pushEvent<QuitEvent>(getServiceId());
        co_return {};
    }

    void addDependencyInstance(IUselessService *, IService *) {
    }

    void removeDependencyInstance(IUselessService *, IService *) {
    }
};
