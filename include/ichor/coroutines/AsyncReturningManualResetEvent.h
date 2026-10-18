///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

// Copied and modified from Lewis Baker's cppcoro

#pragma once

#include <cstdint>
#include <coroutine>
#include <cassert>
#include <exception>
#include <ichor/Defines.h>

namespace Ichor {
    template <typename T>
    class AsyncReturningManualResetEvent;

    template <typename T>
    class AsyncReturningManualResetEventOperation {
    public:

        explicit constexpr AsyncReturningManualResetEventOperation(const AsyncReturningManualResetEvent<T>& event) noexcept : _event(event) {
        }

        [[nodiscard]] bool await_ready() const noexcept {
            return _event.is_set();
        }
        bool await_suspend(std::coroutine_handle<> awaiter) noexcept{
            _awaiter = awaiter;

            const void* const setState = static_cast<const void*>(&_event);

            void* oldState = _event._state;

            if (oldState == setState) {
                // State is now 'set' no need to suspend.
                return false;
            }

            _next = static_cast<AsyncReturningManualResetEventOperation<T>*>(oldState);
            _event._state = static_cast<void*>(this);

            // Successfully queued this waiter to the list.
            return true;
        }
        [[nodiscard]] constexpr T const & await_resume() const noexcept {
            // An event constructed as initially set has no value to return.
            assert(_event._ret != nullptr);
#ifdef ICHOR_USE_HARDENING
            if(_event._ret == nullptr) [[unlikely]] {
                std::terminate();
            }
#endif
            return *_event._ret;
        }

    private:

        friend class AsyncReturningManualResetEvent<T>;

        const AsyncReturningManualResetEvent<T>& _event;
        AsyncReturningManualResetEventOperation* _next{};
        std::coroutine_handle<> _awaiter;

    };

    /// An async manual-reset event is a coroutine synchronisation abstraction
    /// that allows one or more coroutines to wait until some thread calls
    /// set() on the event.
    ///
    /// When a coroutine awaits a 'set' event the coroutine continues without
    /// suspending. Otherwise, if it awaits a 'not set' event the coroutine is
    /// suspended and is later resumed inside the call to 'set()'.
    ///
    /// \seealso async_auto_reset_event
    template <typename T>
    class AsyncReturningManualResetEvent {
    public:

        /// Initialise the event to either 'set' or 'not set' state.
        /// An event that is initially set has no value, awaiting it before calling set() is an error.
        ///
        /// \param initiallySet
        /// If 'true' then initialises the event to the 'set' state, otherwise
        /// initialises the event to the 'not set' state.
        explicit constexpr AsyncReturningManualResetEvent(bool initiallySet = false) noexcept : _state(initiallySet ? static_cast<void*>(this) : nullptr) {
        }

        constexpr ~AsyncReturningManualResetEvent() {
            // There should be no coroutines still awaiting the event.
            assert(_state == nullptr || _state == static_cast<void*>(this));
        }

        // The 'set' state is a pointer to this object and waiters store a reference to it,
        // so copying or moving an event would leave dangling pointers behind.
        AsyncReturningManualResetEvent(const AsyncReturningManualResetEvent&) = delete;
        AsyncReturningManualResetEvent(AsyncReturningManualResetEvent&&) = delete;
        AsyncReturningManualResetEvent& operator=(const AsyncReturningManualResetEvent&) = delete;
        AsyncReturningManualResetEvent& operator=(AsyncReturningManualResetEvent&&) = delete;

        /// Wait for the event to enter the 'set' state.
        ///
        /// If the event is already 'set' then the coroutine continues without
        /// suspending.
        ///
        /// Otherwise, the coroutine is suspended and later resumed when some
        /// thread calls 'set()'. The coroutine will be resumed inside the next
        /// call to 'set()'.
        constexpr AsyncReturningManualResetEventOperation<T> operator co_await() const noexcept {
            return AsyncReturningManualResetEventOperation<T>{ *this };
        }

        /// Query if the event is currently in the 'set' state.
        constexpr bool is_set() const noexcept {
            return _state == static_cast<const void*>(this);
        }

        /// Set the state of the event to 'set'.
        ///
        /// If there are pending coroutines awaiting the event then all
        /// pending coroutines are resumed within this call, in the order they started waiting.
        /// Any coroutines that subsequently await the event will continue
        /// without suspending.
        ///
        /// Only a pointer to t is stored: t has to outlive every co_await on this event until
        /// reset() is called or the event is destroyed. Waiters that are pending during this
        /// call are resumed before it returns, so for them a temporary is fine.
        constexpr void set(T const &t) noexcept {
            _ret = &t;
            void* const setState = static_cast<void*>(this);

            void* oldState = _state;
            _state = setState;
            if (oldState != setState) {
                // Waiters are pushed onto the front of the list, reverse it to resume them in FIFO order.
                AsyncReturningManualResetEventOperation<T>* current{};
                auto* waiter = static_cast<AsyncReturningManualResetEventOperation<T>*>(oldState);
                while (waiter != nullptr) {
                    auto* next = waiter->_next;
                    waiter->_next = current;
                    current = waiter;
                    waiter = next;
                }
                while (current != nullptr) {
                    auto* next = current->_next;

                    current->_awaiter.resume();
                    current = next;
                }
            }
        }

        /// Set the state of the event to 'not-set'.
        ///
        /// Any coroutines that subsequently await the event will suspend
        /// until some thread calls 'set()'.
        ///
        /// This is a no-op if the state was already 'not set'.
        constexpr void reset() noexcept {
            if(_state == static_cast<void*>(this)) {
                _state = nullptr;
            }
            _ret = nullptr;
        }

    private:

        friend class AsyncReturningManualResetEventOperation<T>;

        // This variable has 3 states:
        // - this    - The state is 'set'.
        // - nullptr - The state is 'not set' with no waiters.
        // - other   - The state is 'not set'.
        //             Points to an 'AsyncReturningManualResetEventOperation' that is
        //             the head of a linked-list of waiters.
        mutable void* _state;
        mutable T const * _ret{};
    };
}
