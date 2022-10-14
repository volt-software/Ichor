///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

// Copied and modified from Lewis Baker's cppcoro

#include <atomic>
#include <cstdint>
#include <coroutine>
#include <ichor/coroutines/AsyncGeneratorPromiseBase.h>

namespace Ichor
{
    class AsyncManualResetEventOperation;
    class DependencyManager;

    /// An async manual-reset event is a coroutine synchronisation abstraction
    /// that allows one or more coroutines to wait until some thread calls
    /// set() on the event.
    ///
    /// When a coroutine awaits a 'set' event the coroutine continues without
    /// suspending. Otherwise, if it awaits a 'not set' event the coroutine is
    /// suspended and is later resumed inside the call to 'set()'.
    ///
    /// \seealso async_auto_reset_event
    class AsyncManualResetEvent
    {
    public:

        /// Initialise the event to either 'set' or 'not set' state.
        ///
        /// \param initiallySet
        /// If 'true' then initialises the event to the 'set' state, otherwise
        /// initialises the event to the 'not set' state.
        AsyncManualResetEvent(bool initiallySet = false) noexcept;

        ~AsyncManualResetEvent();

        /// Wait for the event to enter the 'set' state.
        ///
        /// If the event is already 'set' then the coroutine continues without
        /// suspending.
        ///
        /// Otherwise, the coroutine is suspended and later resumed when some
        /// thread calls 'set()'. The coroutine will be resumed inside the next
        /// call to 'set()'.
        AsyncManualResetEventOperation operator co_await() const noexcept;

        /// Query if the event is currently in the 'set' state.
        bool is_set() const noexcept;

        /// Set the state of the event to 'set'.
        ///
        /// If there are pending coroutines awaiting the event then all
        /// pending coroutines are resumed within this call.
        /// Any coroutines that subsequently await the event will continue
        /// without suspending.
        ///
        /// This operation is a no-op if the event was already 'set'.
        void set() noexcept;

        /// Set the state of the event to 'not-set'.
        ///
        /// Any coroutines that subsequently await the event will suspend
        /// until some thread calls 'set()'.
        ///
        /// This is a no-op if the state was already 'not set'.
        void reset() noexcept;

    private:

        friend class AsyncManualResetEventOperation;

        // This variable has 3 states:
        // - this    - The state is 'set'.
        // - nullptr - The state is 'not set' with no waiters.
        // - other   - The state is 'not set'.
        //             Points to an 'AsyncManualResetEventOperation' that is
        //             the head of a linked-list of waiters.
        mutable std::atomic<void*> _state;
//        DependencyManager &_dm;
    };

    class AsyncManualResetEventOperation
    {
    public:

        explicit AsyncManualResetEventOperation(const AsyncManualResetEvent& event) noexcept;

        bool await_ready() const noexcept;
        bool await_suspend(std::coroutine_handle<Detail::AsyncGeneratorPromise<bool>> _awaiter) noexcept;
        void await_resume() const noexcept {}

    private:

        friend class AsyncManualResetEvent;

        const AsyncManualResetEvent& _event;
        AsyncManualResetEventOperation* _next;
        std::coroutine_handle<Detail::AsyncGeneratorPromise<bool>> _awaiter;

    };
}
