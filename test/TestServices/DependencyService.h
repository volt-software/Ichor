#pragma once

#include "UselessService.h"

using namespace Ichor;

struct ICountService {
    [[nodiscard]] virtual uint64_t getSvcCount() const noexcept = 0;
    [[nodiscard]] virtual bool isRunning() const noexcept = 0;
protected:
    ~ICountService() = default;
};

template<bool required>
struct DependencyService final : public ICountService, public AdvancedService<DependencyService<required>> {
    DependencyService(DependencyRegister &reg, Properties props, DependencyManager *mng) : AdvancedService<DependencyService<required>>(std::move(props), mng) {
        reg.registerDependency<IUselessService>(this, required);
    }
    ~DependencyService() final = default;
    AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
        running = true;
        co_return {};
    }
    AsyncGenerator<void> stop() final {
        running = false;
        co_return;
    }

    void addDependencyInstance(IUselessService *, IService *) {
        svcCount++;
    }

    void removeDependencyInstance(IUselessService *, IService *) {
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
