#pragma once

#ifndef ICHOR_USE_LIBURING
#error "Ichor has not been compiled with io_uring support"
#endif

#include <ichor/services/timer/ITimer.h>
#include <ichor/event_queues/IIOUringQueue.h>
#include <ichor/ichor_liburing.h>

namespace Ichor::v1 {
    template <typename TIMER, typename QUEUE>
    class TimerFactory;

    class IOUringTimer final : public ITimer {
    public:
        ///
        /// \param timerId unique identifier for timer
        /// \param svcId unique identifier for svc using this timer
        IOUringTimer(Ichor::ScopedServiceProxy<IIOUringQueue*> queue, uint64_t timerId, ServiceIdType svcId) noexcept;
        IOUringTimer(IOUringTimer const &) = delete;
        IOUringTimer(IOUringTimer &&) noexcept = default;

        ~IOUringTimer() noexcept = default;

        IOUringTimer& operator=(IOUringTimer const &) = delete;
        IOUringTimer& operator=(IOUringTimer &&o) noexcept = default;

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
        std::function<void(io_uring_cqe*)> createNewHandler() noexcept;
        std::function<void(io_uring_cqe*)> createNewUpdateHandler() noexcept;

        template <typename TIMER, typename QUEUE>
        friend class TimerFactory;

        Ichor::ScopedServiceProxy<IIOUringQueue*> _q;
        uint64_t _timerId{};
        TimerState _state{};
        bool _fireOnce{};
        bool _multishotCapable{};
        __kernel_timespec _timespec{};
        std::function<AsyncGenerator<IchorBehaviour>()> _fnAsync{};
        std::function<void()> _fn{};
        uint64_t _priority{INTERNAL_EVENT_PRIORITY};
        ServiceIdType _requestingServiceId{};
        uint64_t _uringTimerId{};
        std::vector<std::function<void(void)>> _quitCbs;
    };
}
