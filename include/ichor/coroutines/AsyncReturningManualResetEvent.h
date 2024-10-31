///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

// Copied and modified from Lewis Baker's cppcoro

#pragma once

#include <cstdint>
#include <coroutine>
#include <cassert>

namespace Ichor {
    template <typename T>
    class AsyncReturningManualResetEvent;

    template <typename T>
    class AsyncReturningManualResetEventOperation {
    public:

        explicit constexpr AsyncReturningManualResetEventOperation(const AsyncReturningManualResetEvent<T>& event) noexcept : _event(event) {
        }

        bool await_ready() const noexcept {
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
        constexpr T const & await_resume() const noexcept {
            return *_event._ret;
        }

    private:

        friend class AsyncReturningManualResetEvent<T>;

        const AsyncReturningManualResetEvent<T>& _event;
        AsyncReturningManualResetEventOperation* _next;
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
        /// pending coroutines are resumed within this call.
        /// Any coroutines that subsequently await the event will continue
        /// without suspending.
        ///
        /// This operation is a no-op if the event was already 'set'.
        constexpr void set(T const &t) noexcept {
            _ret = &t;
            void* const setState = static_cast<void*>(this);

            // Needs 'release' semantics so that prior writes are visible to event awaiters
            // that synchronise either via 'is_set()' or 'operator co_await()'.
            // Needs 'acquire' semantics in case there are any waiters so that we see
            // prior writes to the waiting coroutine's state and to the contents of
            // the queued AsyncReturningManualResetEventOperation objects.
            void* oldState = _state;
            _state = setState;
            if (oldState != setState) {
                auto* current = static_cast<AsyncReturningManualResetEventOperation<T>*>(oldState);
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
        mutable T const * _ret;
    };
}
