#pragma once

#include <ichor/services/timer/ITimerFactory.h>

using namespace Ichor;
using namespace Ichor::v1;

extern std::atomic<uint64_t> evtGate;

class ITimerRunsOnceService {
public:
    [[nodiscard]] virtual uint64_t getCount() const noexcept = 0;
protected:
    ~ITimerRunsOnceService() = default;
};

class TimerRunsOnceService final : public ITimerRunsOnceService {
public:
    explicit TimerRunsOnceService(ScopedServiceProxy<ITimerFactory> factory) {
        auto timer = factory->createTimer();
        timer.setChronoInterval(std::chrono::seconds(1));
        timer.setCallback([this, timer]() mutable {
            evtGate++;
            count++;
            timer.stopTimer({});
        });
        timer.startTimer(true);
    }

    [[nodiscard]] uint64_t getCount() const noexcept final {
        return count;
    }

private:
    uint64_t count{};
};
