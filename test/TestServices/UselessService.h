#pragma once

#include <ichor/dependency_management/Service.h>

namespace Ichor {
    struct IUselessService {
    };

    struct UselessService final : public IUselessService, public Service<UselessService> {
        UselessService() = default;
    };
}
