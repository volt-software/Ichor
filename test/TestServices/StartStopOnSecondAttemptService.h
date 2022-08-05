#pragma once

#include <ichor/Service.h>

using namespace Ichor;

struct StartStopOnSecondAttemptService final : public Service<StartStopOnSecondAttemptService> {
    StartStopOnSecondAttemptService() = default;

    StartBehaviour start() final {
        starts++;
        if(starts == 2) {
            return StartBehaviour::SUCCEEDED;
        }
        return StartBehaviour::FAILED_AND_RETRY;
    }

    StartBehaviour stop() final {
        stops++;
        if(stops == 2) {
            return StartBehaviour::SUCCEEDED;
        }
        return StartBehaviour::FAILED_AND_RETRY;
    }

    uint64_t starts{};
    uint64_t stops{};
};