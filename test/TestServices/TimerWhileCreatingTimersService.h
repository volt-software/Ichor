#pragma once

#include <ichor/services/timer/ITimerFactory.h>
#include "TimerRunsOnceService.h"

using namespace Ichor;
using namespace Ichor::v1;

extern std::atomic<uint64_t> evtGate;

/// Starts a timer and then creates many more, which used to reallocate the factory's storage of the running timer.
class TimerWhileCreatingTimersService final : public ITimerRunsOnceService {
public:
    explicit TimerWhileCreatingTimersService(ScopedServiceProxy<ITimerFactory> factory) : _timer(factory->createTimer()) {
        _timer.setChronoInterval(std::chrono::milliseconds(1));
        _timer.setCallback([this]() {
            count++;
            evtGate++;
        });
        _timer.startTimer(true);

        for(uint32_t i = 0; i < 64; ++i) {
            auto &other = _others.emplace_back(factory->createTimer());
            other.setChronoInterval(std::chrono::milliseconds(1));
            other.setCallback([]() {});
        }
    }

    [[nodiscard]] uint64_t getCount() const noexcept final {
        return count;
    }

private:
    TimerRef _timer;
    std::vector<TimerRef> _others;
    uint64_t count{};
};
