#pragma once

#include <ichor/Service.h>

using namespace Ichor;

struct IUselessService {
};
struct UselessService final : public IUselessService, public Service<UselessService> {
    UselessService() = default;

    StartBehaviour start() final {
        return StartBehaviour::SUCCEEDED;
    }
};