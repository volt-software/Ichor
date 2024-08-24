#pragma once

#include <ichor/dependency_management/AdvancedService.h>

namespace Ichor {
    struct IUselessService {
    };

    struct UselessService final : public IUselessService, public AdvancedService<UselessService> {
        UselessService() = default;
    };

    struct UselessService2 final : public IUselessService, public AdvancedService<UselessService2> {
        UselessService2() = default;
    };

    struct UselessService3 final : public IUselessService, public AdvancedService<UselessService3> {
        UselessService3() = default;
    };
}
