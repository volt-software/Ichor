#pragma once

#include <ichor/Common.h>
#include <ichor/services/timer/ITimer.h>
#include <ichor/services/timer/ITimerFactory.h>

namespace Ichor::v1 {
    // Lightweight reference wrapper to a timer owned by an ITimerFactory.
    // Stores only a timer id and a reference to the factory and delegates all
    // ITimer calls by looking up the timer on each call.
    // This is to have a stable reference to a container that may invalidate references.
    class TimerRef final : public ITimer {
    public:
        TimerRef(ITimerFactory &factory, uint64_t timerId) noexcept : _factory(&factory), _timerId(timerId) {}
        ~TimerRef() = default;

        // // Allow implicit use where an ITimer& is expected (keeps existing call sites working).
        // operator ITimer&() noexcept { return _ensureTimer(); }
        // operator ITimer const&() const noexcept { return _ensureTimer(); }
        // // Allow implicit use where an ITimer* is expected.
        // operator ITimer*() noexcept { return &_ensureTimer(); }
        // operator ITimer const*() const noexcept { return &_ensureTimer(); }

        bool startTimer() final {
            return _ensureTimer().startTimer();
        }

        bool startTimer(bool fireImmediately) final {
            return _ensureTimer().startTimer(fireImmediately);
        }

        bool stopTimer(std::function<void(void)> cb) final {
            return _ensureTimer().stopTimer(std::move(cb));
        }

        [[nodiscard]] TimerState getState() const noexcept final {
            return _ensureTimer().getState();
        }

        void setInterval(uint64_t nanoseconds) noexcept final {
            _ensureTimer().setInterval(nanoseconds);
        }

        void setPriority(uint64_t priority) noexcept final {
            _ensureTimer().setPriority(priority);
        }

        [[nodiscard]] uint64_t getPriority() const noexcept final {
            return _ensureTimer().getPriority();
        }

        void setFireOnce(bool fireOnce) noexcept final {
            _ensureTimer().setFireOnce(fireOnce);
        }

        [[nodiscard]] bool getFireOnce() const noexcept final {
            return _ensureTimer().getFireOnce();
        }

        [[nodiscard]] uint64_t getTimerId() const noexcept final {
            return _timerId;
        }

        [[nodiscard]] ServiceIdType getRequestingServiceId() const noexcept final {
            return _ensureTimer().getRequestingServiceId();
        }

        void setCallbackAsync(std::function<AsyncGenerator<IchorBehaviour>()> fn) final {
            _ensureTimer().setCallbackAsync(std::move(fn));
        }

        void setCallback(std::function<void()> fn) final {
            _ensureTimer().setCallback(std::move(fn));
        }

    private:
        // Helper to resolve the underlying timer; terminates in debug/hardening if not found.
        ITimer& _ensureTimer() const noexcept {
            auto t = _factory->getTimerById(_timerId);
            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (!t) [[unlikely]] {
                    std::terminate();
                }
            }
            return *(*t); // may be UB if nullptr when not hardening, consistent with existing style
        }

        NeverNull<ITimerFactory*> _factory;
        uint64_t _timerId{};
    };
}
