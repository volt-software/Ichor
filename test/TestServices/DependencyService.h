#pragma once

#include "UselessService.h"
#include "ICountService.h"

using namespace Ichor;

template<DependencyFlags flags>
struct DependencyService final : public ICountService, public AdvancedService<DependencyService<flags>> {
    DependencyService(DependencyRegister &reg, Properties props) : AdvancedService<DependencyService<flags>>(std::move(props)) {
        reg.registerDependency<IUselessService>(this, flags);
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

    void addDependencyInstance(IUselessService&, IService&) {
        svcCount++;
    }

    void removeDependencyInstance(IUselessService&, IService&) {
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
