#pragma once

#include <ichor/Defines.h>
#include <ichor/services/timer/ITimer.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <atomic>
#include <memory>

namespace Ichor::v1 {
    namespace Detail {
        /// Shared by the TimerRefs of a timer and the factory owning it.
        struct TimerRefState final {
            TimerRefState(ITimer &_timer, ITimerFactory &_factory, uint64_t _timerId) noexcept : timer(&_timer), factory(&_factory), timerId(_timerId) {}

            /// Set to nullptr by the factory when it destroys the timer, after which the factory may not exist anymore either.
            ITimer *timer;
            ITimerFactory *factory;
            uint64_t timerId;
            /// Amount of TimerRefs owning the timer. Atomic because the thread of a timer copies its callback, which may hold a TimerRef.
            std::atomic<uint64_t> owners{1};
        };
    }

    /// Owning handle to a timer of an ITimerFactory, similar to a std::shared_ptr. When the last TimerRef of a timer is destroyed, the
    /// timer is stopped and destroyed once it has stopped. detach() gives up ownership instead, after which the timer lives until its
    /// factory stops (i.e. until the requesting service is removed).
    /// A TimerRef captured in the callback of its own timer keeps the timer alive until its factory stops, just like detach().
    /// Using a TimerRef after detach(), after being moved from or after its timer is destroyed by its factory (e.g. destroyTimerIfStopped
    /// or the factory stopping) terminates the program.
    /// Not thread-safe: create, use and destroy TimerRefs on the thread of the requesting service.
    // NOLINTNEXTLINE(cppcoreguidelines-virtual-class-destructor): final, and ITimer's destructor is protected: nothing deletes it through another type. Also on the forward declaration in ITimerFactory.h, where clang-tidy reports it when that comes first.
    class TimerRef final : public ITimer {
    public:
        /// Used by factories, the state starts out with this TimerRef as its only owner.
        explicit TimerRef(std::shared_ptr<Detail::TimerRefState> state) noexcept : _state(std::move(state)), _timerId(_state->timerId) {}
        TimerRef(TimerRef const &o) noexcept : _state(o._state), _timerId(o._timerId) {
            if(_state) {
                _state->owners.fetch_add(1, std::memory_order_relaxed);
            }
        }
        TimerRef(TimerRef &&o) noexcept : _state(std::move(o._state)), _timerId(o._timerId) {}
        ~TimerRef() {
            release();
        }

        TimerRef& operator=(TimerRef const &o) noexcept {
            if(this != &o) {
                TimerRef copy{o};
                *this = std::move(copy);
            }
            return *this;
        }

        TimerRef& operator=(TimerRef &&o) noexcept {
            if(this != &o) {
                release();
                _state = std::move(o._state);
                _timerId = o._timerId;
            }
            return *this;
        }

        /// Gives up ownership without stopping the timer. The timer lives until it is destroyed through its factory or until its factory stops.
        void detach() noexcept {
            _state.reset();
        }

        bool startTimer() final {
            return _ensureTimer().startTimer();
        }

        bool startTimer(bool fireImmediately) final {
            return _ensureTimer().startTimer(fireImmediately);
        }

        bool stopTimer(std::function<void()> cb) final {
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

        /// Works even after detach(), being moved from or the timer being destroyed.
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
        void release() noexcept {
            if(!_state) {
                return;
            }
            auto state = std::move(_state);
            // The factory may be gone already if it destroyed the timer
            if(state->owners.fetch_sub(1, std::memory_order_acq_rel) == 1 && state->timer != nullptr) {
                state->factory->releaseTimer(state->timerId);
            }
        }

        [[nodiscard]] ITimer& _ensureTimer() const noexcept {
            if(!_state || _state->timer == nullptr) [[unlikely]] {
                std::terminate();
            }
            return *_state->timer;
        }

        std::shared_ptr<Detail::TimerRefState> _state;
        uint64_t _timerId{};
    };
}
