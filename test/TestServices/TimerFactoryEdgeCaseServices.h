#pragma once

#include <ichor/services/timer/ITimerFactory.h>

using namespace Ichor;
using namespace Ichor::v1;

extern std::atomic<uint64_t> evtGate;
inline std::atomic<uint64_t> countingTimerCallbacks{};

class ITimerEdgeCaseService {
public:
    [[nodiscard]] virtual uint64_t getRestarts() const noexcept = 0;
    virtual void stopTimerAndDestroyItWhenStopped() = 0;
protected:
    ~ITimerEdgeCaseService() = default;
};

/// Has a repeating timer that only counts its callbacks, without touching the service, so that it is safe even if callbacks wrongly
/// keep running after the service stopped.
class CountingTimerService final {
public:
    explicit CountingTimerService(ScopedServiceProxy<ITimerFactory> factory) {
        auto timer = factory->createTimer();
        timer.setChronoInterval(std::chrono::milliseconds(1));
        timer.setCallback([]() {
            countingTimerCallbacks++;
        });
        timer.startTimer();
    }
};

/// A fire-once timer restarting itself from its callback, and a timer whose stop callback destroys it.
class TimerEdgeCaseService final : public ITimerEdgeCaseService {
public:
    explicit TimerEdgeCaseService(ScopedServiceProxy<ITimerFactory> factory) : _factory(factory), _restarting(factory->createTimer()), _destroyedOnStop(factory->createTimer()) {
        _restarting.setChronoInterval(std::chrono::milliseconds(1));
        _restarting.setFireOnce(true);
        _restarting.setCallback([this]() {
            if(++_restarts < 3) {
                if(!_restarting.startTimer()) {
                    std::terminate();
                }
            } else {
                evtGate++;
            }
        });
        _restarting.startTimer();

        // The thread based Timer is STOPPING for up to a tenth of its interval, long enough for stopAllTimers to start in between.
        _destroyedOnStop.setChronoInterval(std::chrono::milliseconds(200));
        _destroyedOnStop.setCallback([]() {});
        _destroyedOnStop.startTimer();
    }

    [[nodiscard]] uint64_t getRestarts() const noexcept final {
        return _restarts;
    }

    void stopTimerAndDestroyItWhenStopped() final {
        _destroyedOnStop.stopTimer([factory = _factory, id = _destroyedOnStop.getTimerId()]() mutable {
            factory->destroyTimerIfStopped(id);
        });
    }

private:
    ScopedServiceProxy<ITimerFactory> _factory;
    TimerRef _restarting;
    TimerRef _destroyedOnStop;
    uint64_t _restarts{};
};
