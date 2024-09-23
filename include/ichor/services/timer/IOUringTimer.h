#pragma once

#ifndef ICHOR_USE_LIBURING
#error "Ichor has not been compiled with io_uring support"
#endif

#include <ichor/services/timer/ITimer.h>
#include <ichor/event_queues/IIOUringQueue.h>
#include <ichor/ichor_liburing.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>

namespace Ichor {
    template <typename TIMER, typename QUEUE>
    class TimerFactory;

    // Rather shoddy implementation, setting the interval does not reset the insertEventLoop function and the sleep_for is sketchy at best.
    class IOUringTimer final : public ITimer {
    public:
        ~IOUringTimer() noexcept = default;

        /// Thread-safe.
        void startTimer() final;
        /// Thread-safe.
        void startTimer(bool fireImmediately) final;
        /// Thread-safe.
        void stopTimer() final;
//        void stopTimer(std::function<void()>) final;

        /// Thread-safe.
        [[nodiscard]] bool running() const noexcept final;

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

    private:
        ///
        /// \param timerId unique identifier for timer
        /// \param svcId unique identifier for svc using this timer
        IOUringTimer(IIOUringQueue& queue, uint64_t timerId, uint64_t svcId) noexcept;
        std::function<void(io_uring_cqe*)> createNewHandler() noexcept;

        template <typename TIMER, typename QUEUE>
        friend class TimerFactory;

        IIOUringQueue& _q;
        uint64_t _timerId{};
        bool _fireOnce{};
        __kernel_timespec _timespec{};
        std::function<AsyncGenerator<IchorBehaviour>()> _fnAsync{};
        std::function<void()> _fn{};
//        std::function<void()> _stopCb{};
        bool _quit{true};
        uint64_t _priority{INTERNAL_EVENT_PRIORITY};
        uint64_t _requestingServiceId{};
        uint64_t _uringTimerId{};
    };
}
