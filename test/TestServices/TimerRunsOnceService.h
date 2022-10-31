#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/timer/TimerService.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>

using namespace Ichor;

class TimerRunsOnceService final : public Service<TimerRunsOnceService> {
public:
    TimerRunsOnceService() = default;
    ~TimerRunsOnceService() final = default;

    StartBehaviour start() final {
        fmt::print("start\n");
        _timerManager = getManager().createServiceManager<Timer, ITimer>();
        _timerManager->setChronoInterval(std::chrono::milliseconds(5));
        _timerManager->setCallback(this, [this](DependencyManager &dm) -> AsyncGenerator<void> {
            count++;
            _timerManager->stopTimer();
            co_return;
        });
        _timerManager->startTimer(true);
        return StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        _timerManager = nullptr;
        return StartBehaviour::SUCCEEDED;
    }

    uint64_t count{};
private:
    Timer* _timerManager{};

};