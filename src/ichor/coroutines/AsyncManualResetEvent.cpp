///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

// Copied and modified from Lewis Baker's cppcoro

#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <cassert>

Ichor::AsyncManualResetEvent::AsyncManualResetEvent(bool initiallySet) noexcept
        : _state(initiallySet ? static_cast<void*>(this) : nullptr)
{}

Ichor::AsyncManualResetEvent::~AsyncManualResetEvent() {
    // There should be no coroutines still awaiting the event.
    assert(
            _state == nullptr ||
            _state == static_cast<void*>(this));
}

bool Ichor::AsyncManualResetEvent::is_set() const noexcept {
    return _state == static_cast<const void*>(this);
}

Ichor::AsyncManualResetEventOperation
        Ichor::AsyncManualResetEvent::operator co_await() const noexcept {
    return AsyncManualResetEventOperation{ *this };
}

void Ichor::AsyncManualResetEvent::set() noexcept {
    void* const setState = static_cast<void*>(this);

    // Needs 'release' semantics so that prior writes are visible to event awaiters
    // that synchronise either via 'is_set()' or 'operator co_await()'.
    // Needs 'acquire' semantics in case there are any waiters so that we see
    // prior writes to the waiting coroutine's state and to the contents of
    // the queued AsyncManualResetEventOperation objects.
    void* oldState = _state;
    _state = setState;
    if (oldState != setState) {
        auto* current = static_cast<AsyncManualResetEventOperation*>(oldState);
        while (current != nullptr) {
            auto* next = current->_next;
            current->_awaiter.resume();
            current = next;
        }
    }
}

void Ichor::AsyncManualResetEvent::reset() noexcept {
    if(_state == static_cast<void*>(this)) {
        _state = nullptr;
    }
}

Ichor::AsyncManualResetEventOperation::AsyncManualResetEventOperation(
        const AsyncManualResetEvent& event) noexcept
        : _event(event)
{
}

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
