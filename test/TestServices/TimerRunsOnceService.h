#pragma once

#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/dependency_management/AdvancedService.h>

using namespace Ichor;

class ITimerRunsOnceService {
public:
    virtual uint64_t getCount() const noexcept = 0;
protected:
    ~ITimerRunsOnceService() = default;
};

class TimerRunsOnceService final : public ITimerRunsOnceService {
public:
    TimerRunsOnceService(ITimerFactory *factory) {
        auto &timer = factory->createTimer();
        timer.setChronoInterval(std::chrono::seconds(1));
        timer.setCallback([this, &timer = timer](DependencyManager &) {
            count++;
            timer.stopTimer();
        });
        timer.startTimer(true);
    }

    uint64_t getCount() const noexcept final {
        return count;
    }

private:
    uint64_t count{};
};
