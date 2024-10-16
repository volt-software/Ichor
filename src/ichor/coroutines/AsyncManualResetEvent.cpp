///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

// Copied and modified from Lewis Baker's cppcoro

#include <ichor/coroutines/AsyncManualResetEvent.h>

bool Ichor::AsyncManualResetEventOperation::await_ready() const noexcept {
    return _event.is_set();
}

bool Ichor::AsyncManualResetEventOperation::await_suspend(std::coroutine_handle<> awaiter) noexcept {
    _awaiter = awaiter;

    const void* const setState = static_cast<const void*>(&_event);

    void* oldState = _event._state;

    if (oldState == setState) {
        // State is now 'set' no need to suspend.
        return false;
    }

    _next = static_cast<AsyncManualResetEventOperation*>(oldState);
    _event._state = static_cast<void*>(this);

    // Successfully queued this waiter to the list.
    return true;
}
