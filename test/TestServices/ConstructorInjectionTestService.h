#pragma once

#include "UselessService.h"
#include "DependencyService.h"

using namespace Ichor;

struct ConstructorInjectionTestService final : public ICountService {
    ConstructorInjectionTestService(IService* ourselves, ILogger *svc) {

    }

    [[nodiscard]] uint64_t getSvcCount() const noexcept final {
        return svcCount;
    }

    [[nodiscard]] bool isRunning() const noexcept final {
        return running;
    }

    uint64_t svcCount{1};
    bool running{true};
};
