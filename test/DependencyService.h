#pragma once

#include "UselessService.h"

using namespace Ichor;

struct ICountService {
    [[nodiscard]] virtual uint64_t getSvcCount() const noexcept = 0;
};

template<bool required>
struct DependencyService final : public ICountService, public Service<DependencyService<required>> {
    DependencyService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service<DependencyService<required>>(std::move(props), mng) {
        reg.registerDependency<IUselessService>(this, required);
    }
    ~DependencyService() final = default;
    bool start() final {
        return true;
    }
    bool stop() final {
        return true;
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

    uint64_t svcCount{};
};