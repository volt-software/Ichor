#pragma once

#include <ichor/services/timer/TimerService.h>
#include <ichor/dependency_management/AdvancedService.h>

using namespace Ichor;

class TimerRunsOnceService final : public AdvancedService<TimerRunsOnceService> {
public:
    TimerRunsOnceService() = default;
    ~TimerRunsOnceService() final = default;

    Task<tl::expected<void, Ichor::StartError>> start() final {
        fmt::print("start\n");
        _timerManager = GetThreadLocalManager().createServiceManager<Timer, ITimer>();
        _timerManager->setChronoInterval(std::chrono::milliseconds(5));
        _timerManager->setCallback(this, [this](DependencyManager &dm) {
            count++;
            _timerManager->stopTimer();
        });
        _timerManager->startTimer(true);
        co_return {};
    }

    Task<void> stop() final {
        _timerManager = nullptr;
        co_return;
    }

    uint64_t count{};
private:
    Timer* _timerManager{};

};
