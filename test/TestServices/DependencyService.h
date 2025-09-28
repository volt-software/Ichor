#pragma once

#include "UselessService.h"
#include "ICountService.h"
#include <ichor/ServiceExecutionScope.h>

using namespace Ichor;
using namespace Ichor::v1;

template<typename DependencyType, DependencyFlags flags>
struct DependencyService final : public ICountService, public AdvancedService<DependencyService<DependencyType, flags>> {
    DependencyService(DependencyRegister &reg, Properties props) : AdvancedService<DependencyService<DependencyType, flags>>(std::move(props)) {
        reg.registerDependency<DependencyType>(this, flags);
    }
    ~DependencyService() final = default;
    Task<tl::expected<void, Ichor::StartError>> start() final {
        running = true;
        co_return {};
    }
    Task<void> stop() final {
        running = false;
        co_return;
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<DependencyType*>, IService&) {
        svcCount++;
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<DependencyType*>, IService&) {
        svcCount--;
    }

    [[nodiscard]] uint64_t getSvcCount() const noexcept final {
        return svcCount;
    }

    [[nodiscard]] bool isRunning() const noexcept final {
        return running;
    }

    uint64_t svcCount{};
    bool running{};
};
