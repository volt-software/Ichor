#pragma once

#include "UselessService.h"

using namespace Ichor;

struct ICountService {
    virtual ~ICountService() = default;
    [[nodiscard]] virtual uint64_t getSvcCount() const noexcept = 0;
    [[nodiscard]] virtual bool isRunning() const noexcept = 0;
};

template<bool required>
struct DependencyService final : public ICountService, public Service<DependencyService<required>> {
    DependencyService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service<DependencyService<required>>(std::move(props), mng) {
        reg.registerDependency<IUselessService>(this, required);
    }
    ~DependencyService() final = default;
    StartBehaviour start() final {
        running = true;
        return StartBehaviour::SUCCEEDED;
    }
    StartBehaviour stop() final {
        running = false;
        return StartBehaviour::SUCCEEDED;
    }

    void addDependencyInstance(IUselessService *svc, IService *) {
        svcCount++;
    }

    void removeDependencyInstance(IUselessService *svc, IService *) {
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