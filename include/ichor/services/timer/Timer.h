#pragma once

#include <thread>
#include <ichor/services/timer/ITimer.h>
#include <ichor/event_queues/IEventQueue.h>
#include <ichor/stl/RealtimeMutex.h>

namespace Ichor::v1 {
    template <typename TIMER, typename QUEUE>
    class TimerFactory;

    // Rather shoddy implementation, setting the interval does not reset the insertEventLoop function and the sleep_for is sketchy at best.
    class Timer final : public ITimer {
    public:
        ///
        /// \param timerId unique identifier for timer
        /// \param svcId unique identifier for svc using this timer
        Timer(IEventQueue& queue, uint64_t timerId, ServiceIdType svcId) noexcept;
        Timer(Timer const &) = delete;
        Timer(Timer &&) noexcept = default;

        ~Timer() noexcept;

        Timer& operator=(Timer const &) = delete;
        Timer& operator=(Timer &&o) noexcept = default;

        /// Thread-safe.
        bool startTimer() final;
        /// Thread-safe.
        bool startTimer(bool fireImmediately) final;
        /// Thread-safe.
        bool stopTimer(std::function<void(void)> cb) final;

        /// Thread-safe.
        [[nodiscard]] TimerState getState() const noexcept final;

        /// Sets coroutine based callback, adds some overhead compared to sync version. Executed when timer expires. Terminates program if timer is running.
        /// \param fn callback
        void setCallbackAsync(std::function<AsyncGenerator<IchorBehaviour>()> fn) final;

        /// Set sync callback to execute when timer expires. Terminates program if timer is running.
        /// \param fn callback
        void setCallback(std::function<void()> fn) final;
        /// Thread-safe.
        void setInterval(uint64_t nanoseconds) noexcept final;

        /// Thread-safe.
        void setPriority(uint64_t priority) noexcept final;
        /// Thread-safe.
        [[nodiscard]] uint64_t getPriority() const noexcept final;
        /// Thread-safe.
        void setFireOnce(bool fireOnce) noexcept final;
        /// Thread-safe.
        [[nodiscard]] bool getFireOnce() const noexcept final;
        /// Thread-safe.
        [[nodiscard]] uint64_t getTimerId() const noexcept final;

        [[nodiscard]] ServiceIdType getRequestingServiceId() const noexcept final;

    private:

        void insertEventLoop(bool fireImmediately);

        // template <typename TIMER, typename QUEUE>
        // friend class TimerFactory;

        NeverNull<IEventQueue*> _queue;
        uint64_t _timerId{};
        TimerState _state{};
        bool _fireOnce{};
        uint64_t _intervalNanosec{1'000'000'000};
        std::unique_ptr<std::thread> _eventInsertionThread{};
        std::function<AsyncGenerator<IchorBehaviour>()> _fnAsync{};
        std::function<void()> _fn{};
        uint64_t _priority{INTERNAL_EVENT_PRIORITY};
        ServiceIdType _requestingServiceId{};
        std::vector<std::function<void(void)>> _quitCbs;
        mutable RealtimeMutex _m;
    };
}
