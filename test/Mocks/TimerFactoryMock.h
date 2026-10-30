#pragma once

#include <ichor/services/timer/ITimerFactory.h>
#include <memory>
#include <vector>

/// A timer that only fires when the test says so
struct TimerMock final : public ITimer {
    explicit TimerMock(uint64_t id) noexcept : _id(id) {}

    bool startTimer() final {
        running = true;
        return true;
    }

    bool startTimer(bool) final {
        running = true;
        return true;
    }

    bool stopTimer(std::function<void()> cb) final {
        running = false;
        if(cb) {
            cb();
        }
        return true;
    }

    [[nodiscard]] TimerState getState() const noexcept final {
        return running ? TimerState::RUNNING : TimerState::STOPPED;
    }

    void setInterval(uint64_t nanoseconds) noexcept final {
        interval = std::chrono::nanoseconds{nanoseconds};
    }

    void setPriority(uint64_t) noexcept final {
    }

    [[nodiscard]] uint64_t getPriority() const noexcept final {
        return 0;
    }

    void setFireOnce(bool _fireOnce) noexcept final {
        fireOnce = _fireOnce;
    }

    [[nodiscard]] bool getFireOnce() const noexcept final {
        return fireOnce;
    }

    [[nodiscard]] uint64_t getTimerId() const noexcept final {
        return _id;
    }

    [[nodiscard]] ServiceIdType getRequestingServiceId() const noexcept final {
        return ServiceIdType{0};
    }

    void setCallbackAsync(std::function<AsyncGenerator<IchorBehaviour>()>) final {
        std::terminate();
    }

    void setCallback(std::function<void()> fn) final {
        callback = std::move(fn);
    }

    std::function<void()> callback;
    std::chrono::nanoseconds interval{};
    bool running{};
    bool fireOnce{};

private:
    uint64_t _id;
};

struct TimerFactoryMock final : public ITimerFactory, public AdvancedService<TimerFactoryMock> {
    TimerFactoryMock(DependencyRegister &, Properties props) : AdvancedService<TimerFactoryMock>(std::move(props)) {}
    ~TimerFactoryMock() final {
        for(auto &state : states) {
            state->timer = nullptr;
        }
    }

    [[nodiscard]] TimerRef createTimer() final {
        auto &timer = timers.emplace_back(std::make_unique<TimerMock>(timers.size() + 1));
        auto &state = states.emplace_back(std::make_shared<Detail::TimerRefState>(*timer, *this, timer->getTimerId()));
        return TimerRef{state};
    }

    [[nodiscard]] tl::optional<NeverNull<ITimer*>> getTimerById(uint64_t timerId) noexcept final {
        // Like the real factory in debug builds
        if(stopping) {
            std::terminate();
        }
        if(timerId == 0 || timerId > timers.size()) {
            return tl::nullopt;
        }
        return NeverNull<ITimer*>{timers[timerId - 1].get()};
    }

    bool destroyTimerIfStopped(uint64_t) final {
        return true;
    }

    void releaseTimer(uint64_t timerId) noexcept final {
        // Like the real factory, which leaves stopping and destroying the timers to stopAllTimers while stopping
        if(stopping) {
            return;
        }
        // Stopped but kept around, so that tests can still inspect it
        timers[timerId - 1]->running = false;
        released++;
    }

    [[nodiscard]] VectorView<ITimer> getCreatedTimers() const noexcept final {
        return VectorView<ITimer>{timers};
    }

    [[nodiscard]] ServiceIdType getRequestingServiceId() const noexcept final {
        return ServiceIdType{0};
    }

    /// Fires the running timers
    void fire() {
        for(auto &timer : timers) {
            if(timer->running && timer->callback) {
                timer->callback();
            }
        }
    }

    std::vector<std::unique_ptr<TimerMock>> timers;
    std::vector<std::shared_ptr<Detail::TimerRefState>> states;
    /// Like a factory that is stopping or stopped, which stops and destroys the timers of the service by itself
    bool stopping{};
    /// Amount of timers whose last TimerRef was destroyed
    uint64_t released{};
};
