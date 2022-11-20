#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/timer/TimerService.h>
#include <ichor/Service.h>
#include "ichor/dependency_management/ILifecycleManager.h"

using namespace Ichor;

class TimerRunsOnceService final : public Service<TimerRunsOnceService> {
public:
    TimerRunsOnceService() = default;
    ~TimerRunsOnceService() final = default;

    AsyncGenerator<void> start() final {
        fmt::print("start\n");
        _timerManager = getManager().createServiceManager<Timer, ITimer>();
        _timerManager->setChronoInterval(std::chrono::milliseconds(5));
        _timerManager->setCallback(this, [this](DependencyManager &dm) -> AsyncGenerator<IchorBehaviour> {
            count++;
            _timerManager->stopTimer();
            co_return {};
        });
        _timerManager->startTimer(true);
        co_return;
    }

    AsyncGenerator<void> stop() final {
        _timerManager = nullptr;
        co_return;
    }

    uint64_t count{};
private:
    Timer* _timerManager{};

};