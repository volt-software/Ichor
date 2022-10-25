///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

// Copied and modified from Lewis Baker's cppcoro

#pragma once

#include <atomic>
#include <cstdint>
#include <coroutine>

namespace Ichor {
    class AsyncAutoResetEventOperation;

    /// An async auto-reset event is a coroutine synchronisation abstraction
    /// that allows one or more coroutines to wait until some thread calls
    /// set() on the event.
    ///
    /// When a coroutine awaits a 'set' event the event is automatically
    /// reset back to the 'not set' state, thus the name 'auto reset' event.
    class AsyncAutoResetEvent {
    public:

        /// Initialise the event to either 'set' or 'not set' state.
        AsyncAutoResetEvent(bool initiallySet = false) noexcept;

        ~AsyncAutoResetEvent();

        /// Wait for the event to enter the 'set' state.
        ///
        /// If the event is already 'set' then the event is set to the 'not set'
        /// state and the awaiting coroutine continues without suspending.
        /// Otherwise, the coroutine is suspended and later resumed when some
        /// thread calls 'set()'.
        ///
        /// Note that the coroutine may be resumed inside a call to 'set()'
        /// or inside another thread's call to 'operator co_await()'.
        AsyncAutoResetEventOperation operator co_await() const noexcept;

        /// Set the state of the event to 'set'.
        ///
        /// If there are pending coroutines awaiting the event then one
        /// pending coroutine is resumed and the state is immediately
        /// set back to the 'not set' state.
        ///
        /// This operation is a no-op if the event was already 'set'.
        void set() noexcept;

        /// Set the state of the event to 'set' and run all pending coroutines.
        ///
        /// This operation is a no-op if the event was already 'set'.
        void set_all() noexcept;

        /// Set the state of the event to 'not-set'.
        ///
        /// This is a no-op if the state was already 'not set'.
        void reset() noexcept;

    private:

        friend class AsyncAutoResetEventOperation;

        void resume_waiters(std::uint64_t initialState) const noexcept;

        // Bits 0-31  - Set count
        // Bits 32-63 - Waiter count
        mutable std::atomic<std::uint64_t> m_state;

        mutable std::atomic<AsyncAutoResetEventOperation*> m_newWaiters;

        mutable AsyncAutoResetEventOperation* m_waiters;

    };

    class AsyncAutoResetEventOperation {
    public:

        AsyncAutoResetEventOperation() noexcept;

        explicit AsyncAutoResetEventOperation(const AsyncAutoResetEvent& event) noexcept;

        AsyncAutoResetEventOperation(const AsyncAutoResetEventOperation& other) noexcept;

        bool await_ready() const noexcept { return m_event == nullptr; }
        bool await_suspend(std::coroutine_handle<> awaiter) noexcept;
        void await_resume() const noexcept {}

    private:

        friend class AsyncAutoResetEvent;

        const AsyncAutoResetEvent* m_event;
        AsyncAutoResetEventOperation* m_next{};
        std::coroutine_handle<> m_awaiter;
        std::atomic<std::uint32_t> m_refCount;

    };
}
