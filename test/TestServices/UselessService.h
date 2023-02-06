#pragma once

#include <ichor/dependency_management/AdvancedService.h>

namespace Ichor {
    struct IUselessService {
    };

    struct UselessService final : public IUselessService, public AdvancedService<UselessService> {
        UselessService() = default;
    };
}
