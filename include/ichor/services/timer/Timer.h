#pragma once

#include <thread>
#include <ichor/services/timer/ITimer.h>
#include <ichor/events/RunFunctionEvent.h>

namespace Ichor {
    class TimerFactory;

    // Rather shoddy implementation, setting the interval does not reset the insertEventLoop function and the sleep_for is sketchy at best.
    class Timer final : public ITimer {
    public:
        ~Timer() noexcept;

        /// Thread-safe.
        void startTimer() final;
        /// Thread-safe.
        void startTimer(bool fireImmediately) final;
        /// Thread-safe.
        void stopTimer() final;

        /// Thread-safe.
        [[nodiscard]] bool running() const noexcept final;

        /// Sets coroutine based callback, adds some overhead compared to sync version. Executed when timer expires. Terminates program if timer is running.
        /// \param fn callback
        void setCallbackAsync(decltype(RunFunctionEventAsync::fun) fn) final;

        /// Set sync callback to execute when timer expires. Terminates program if timer is running.
        /// \param fn callback
        void setCallback(decltype(RunFunctionEvent::fun) fn) final;
        /// Thread-safe.
        void setInterval(uint64_t nanoseconds) noexcept final;

        /// Thread-safe.
        void setPriority(uint64_t priority) noexcept final;
        /// Thread-safe.
        [[nodiscard]] uint64_t getPriority() const noexcept final;
        /// Thread-safe.
        [[nodiscard]] uint64_t getTimerId() const noexcept final;

    private:
        ///
        /// \param timerId unique identifier for timer
        /// \param svcId unique identifier for svc using this timer
        Timer(IEventQueue *queue, uint64_t timerId, uint64_t svcId) noexcept;

        void insertEventLoop(bool fireImmediately);

        friend class TimerFactory;

        IEventQueue *_queue{};
        uint64_t _timerId{};
        std::atomic<uint64_t> _intervalNanosec{1'000'000'000};
        std::unique_ptr<std::thread> _eventInsertionThread{};
        decltype(RunFunctionEventAsync::fun) _fnAsync{};
        decltype(RunFunctionEvent::fun) _fn{};
        std::atomic<bool> _quit{true};
        std::atomic<uint64_t> _priority{INTERNAL_EVENT_PRIORITY};
        uint64_t _requestingServiceId{};
    };
}
