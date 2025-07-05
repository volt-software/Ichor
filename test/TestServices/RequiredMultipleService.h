#pragma once

#include "UselessService.h"
#include "ICountService.h"

using namespace Ichor;
using namespace Ichor::v1;

struct RequiredMultipleService final : public ICountService, public AdvancedService<RequiredMultipleService> {
    RequiredMultipleService(DependencyRegister &reg, Properties props) : AdvancedService<RequiredMultipleService>(std::move(props)) {
        reg.registerDependency<IUselessService>(this, DependencyFlags::REQUIRED | DependencyFlags::ALLOW_MULTIPLE);
    }
    ~RequiredMultipleService() final = default;
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

struct RequiredMultipleService2 final : public ICountService, public AdvancedService<RequiredMultipleService2> {
    RequiredMultipleService2(DependencyRegister &reg, Properties props) : AdvancedService<RequiredMultipleService2>(std::move(props)) {
        reg.registerDependency<IUselessService>(this, DependencyFlags::REQUIRED);
        reg.registerDependency<IUselessService>(this, DependencyFlags::REQUIRED);
    }
    ~RequiredMultipleService2() final = default;
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
