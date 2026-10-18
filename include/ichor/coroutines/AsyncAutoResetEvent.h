///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

// Copied and modified from Lewis Baker's cppcoro

#pragma once

#include <coroutine>

namespace Ichor {
    class AsyncAutoResetEventOperation;

    /// An async auto-reset event is a coroutine synchronisation abstraction
    /// that allows one or more coroutines to wait until some coroutine calls
    /// set() on the event.
    ///
    /// When a coroutine awaits a 'set' event the event is automatically
    /// reset back to the 'not set' state, thus the name 'auto reset' event.
    ///
    /// Ichor forces everything to be on the same thread (or terminates the program),
    /// so unlike cppcoro, this event is not thread-safe.
    class AsyncAutoResetEvent {
    public:

        /// Initialise the event to either 'set' or 'not set' state.
        AsyncAutoResetEvent(bool initiallySet = false) noexcept;

        ~AsyncAutoResetEvent();

        // Waiters store a pointer to this object.
        AsyncAutoResetEvent(const AsyncAutoResetEvent&) = delete;
        AsyncAutoResetEvent(AsyncAutoResetEvent&&) = delete;
        AsyncAutoResetEvent& operator=(const AsyncAutoResetEvent&) = delete;
        AsyncAutoResetEvent& operator=(AsyncAutoResetEvent&&) = delete;

        /// Wait for the event to enter the 'set' state.
        ///
        /// If the event is already 'set' then the event is set to the 'not set'
        /// state and the awaiting coroutine continues without suspending.
        /// Otherwise, the coroutine is suspended and later resumed when some
        /// coroutine calls 'set()'.
        AsyncAutoResetEventOperation operator co_await() const noexcept;

        /// Set the state of the event to 'set'.
        ///
        /// If there are pending coroutines awaiting the event then the
        /// longest waiting one is resumed within this call and the event
        /// stays in the 'not set' state.
        ///
        /// This operation is a no-op if the event was already 'set'.
        void set() noexcept;

        /// Resume all pending coroutines, in the order they started waiting,
        /// then set the state of the event to 'set'. Coroutines that await the
        /// event again while being resumed are resumed as well.
        ///
        /// This operation is a no-op if the event was already 'set'.
        void set_all() noexcept;

        /// Set the state of the event to 'not-set'.
        ///
        /// This is a no-op if the state was already 'not set'.
        void reset() noexcept;

    private:

        friend class AsyncAutoResetEventOperation;

        /// Removes the longest waiting coroutine from the queue and resumes it.
        void resume_one() const noexcept;

        mutable bool m_set;

        // FIFO queue of waiters, linked through AsyncAutoResetEventOperation::m_next.
        mutable AsyncAutoResetEventOperation* m_waitersHead{nullptr};
        mutable AsyncAutoResetEventOperation* m_waitersTail{nullptr};
    };

    class AsyncAutoResetEventOperation {
    public:

        AsyncAutoResetEventOperation() noexcept;

        explicit AsyncAutoResetEventOperation(const AsyncAutoResetEvent& event) noexcept;

        AsyncAutoResetEventOperation(const AsyncAutoResetEventOperation& other) noexcept;

        [[nodiscard]] bool await_ready() const noexcept { return m_event == nullptr; }
        void await_suspend(std::coroutine_handle<> awaiter) noexcept;
        void await_resume() const noexcept {}

    private:

        friend class AsyncAutoResetEvent;

        const AsyncAutoResetEvent* m_event;
        AsyncAutoResetEventOperation* m_next{};
        std::coroutine_handle<> m_awaiter;
    };
}
