#pragma once

#include <ichor/services/timer/ITimerFactory.h>

using namespace Ichor;
using namespace Ichor::v1;

extern std::atomic<uint64_t> evtGate;

class ITimerStateTransitionsService {
public:
    [[nodiscard]] virtual TimerRef& getSelfStoppingTimer() noexcept = 0;
    [[nodiscard]] virtual TimerRef& getFireOnceTimer() noexcept = 0;
    [[nodiscard]] virtual TimerRef& getFastTimer() noexcept = 0;
    [[nodiscard]] virtual uint64_t getSelfStoppedCallbacks() const noexcept = 0;
    [[nodiscard]] virtual uint64_t getFireOnceFired() const noexcept = 0;
    [[nodiscard]] virtual uint64_t getEarlyStopCallbacks() const noexcept = 0;
protected:
    ~ITimerStateTransitionsService() = default;
};

/// A repeating timer that stops itself from its callback, and a fire-once timer.
class TimerStateTransitionsService final : public ITimerStateTransitionsService {
public:
    TimerStateTransitionsService(ScopedServiceProxy<ITimerFactory> factory) : _selfStopping(factory->createTimer()), _fireOnce(factory->createTimer()), _fast(factory->createTimer()) {
        _selfStopping.setChronoInterval(std::chrono::milliseconds(1));
        _selfStopping.setCallback([this]() {
            // A long interval, so that waiting on a re-armed timeout or on stale requests would take longer than the test allows.
            _selfStopping.setChronoInterval(std::chrono::seconds(10));
            // stopping twice: the second callback also has to wait until the timer actually stopped
            for(uint32_t i = 0; i < 2; ++i) {
                _selfStopping.stopTimer([this]() {
                    if(_selfStopping.getState() != TimerState::STOPPED) {
                        _earlyStopCallbacks++;
                    }
                    _selfStoppedCallbacks++;
                    evtGate++;
                });
            }
        });
        _selfStopping.startTimer();

        _fireOnce.setChronoInterval(std::chrono::milliseconds(1));
        _fireOnce.setFireOnce(true);
        _fireOnce.setCallback([this]() {
            _fireOnceFired++;
            evtGate++;
        });
        _fireOnce.startTimer();

        // fires faster than the event loop goes around, stopping it must still finish
        _fast.setInterval(100'000);
        _fast.setCallback([this]() {
            if(++_fastFired == 10) {
                _fast.stopTimer([]() {
                    evtGate++;
                });
            }
        });
        _fast.startTimer();
    }

    [[nodiscard]] TimerRef& getSelfStoppingTimer() noexcept final {
        return _selfStopping;
    }

    [[nodiscard]] TimerRef& getFireOnceTimer() noexcept final {
        return _fireOnce;
    }

    [[nodiscard]] TimerRef& getFastTimer() noexcept final {
        return _fast;
    }

    [[nodiscard]] uint64_t getSelfStoppedCallbacks() const noexcept final {
        return _selfStoppedCallbacks;
    }

    [[nodiscard]] uint64_t getFireOnceFired() const noexcept final {
        return _fireOnceFired;
    }

    [[nodiscard]] uint64_t getEarlyStopCallbacks() const noexcept final {
        return _earlyStopCallbacks;
    }

private:
    TimerRef _selfStopping;
    TimerRef _fireOnce;
    TimerRef _fast;
    uint64_t _fastFired{};
    uint64_t _selfStoppedCallbacks{};
    uint64_t _fireOnceFired{};
    uint64_t _earlyStopCallbacks{};
};
