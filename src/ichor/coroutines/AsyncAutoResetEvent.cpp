///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

// Copied and modified from Lewis Baker's cppcoro

#include <ichor/coroutines/AsyncAutoResetEvent.h>

#include <cassert>

Ichor::AsyncAutoResetEvent::AsyncAutoResetEvent(bool initiallySet) noexcept
        : m_set(initiallySet)
        , m_waitersHead(nullptr)
        , m_waitersTail(nullptr)
{
}

Ichor::AsyncAutoResetEvent::~AsyncAutoResetEvent() {
    // There should be no coroutines still awaiting the event.
    assert(m_waitersHead == nullptr);
}

Ichor::AsyncAutoResetEventOperation Ichor::AsyncAutoResetEvent::operator co_await() const noexcept {
    // The event can only be set while nobody is waiting, as set() hands the event directly to a waiter.
    if (m_set) {
        assert(m_waitersHead == nullptr);
        m_set = false;
        // Acquired the event, return an operation object that won't suspend.
        return AsyncAutoResetEventOperation{};
    }

    return AsyncAutoResetEventOperation{ *this };
}

void Ichor::AsyncAutoResetEvent::set() noexcept {
    if (m_waitersHead == nullptr) {
        m_set = true;
        return;
    }

    resume_one();
}

void Ichor::AsyncAutoResetEvent::set_all() noexcept {
    if (m_set) {
        return;
    }

    while (m_waitersHead != nullptr) {
        resume_one();
    }

    m_set = true;
}

void Ichor::AsyncAutoResetEvent::reset() noexcept {
    m_set = false;
}

void Ichor::AsyncAutoResetEvent::resume_one() const noexcept {
    auto* waiter = m_waitersHead;
    assert(waiter != nullptr);

    // Dequeue before resuming, as the resumed coroutine might await this event again or destroy the operation.
    m_waitersHead = waiter->m_next;
    if (m_waitersHead == nullptr) {
        m_waitersTail = nullptr;
    }
    waiter->m_next = nullptr;

    waiter->m_awaiter.resume();
}

Ichor::AsyncAutoResetEventOperation::AsyncAutoResetEventOperation() noexcept
        : m_event(nullptr)
{}

Ichor::AsyncAutoResetEventOperation::AsyncAutoResetEventOperation(const AsyncAutoResetEvent& event) noexcept
        : m_event(&event)
{}

Ichor::AsyncAutoResetEventOperation::AsyncAutoResetEventOperation(const AsyncAutoResetEventOperation& other) noexcept
        : m_event(other.m_event)
{}

void Ichor::AsyncAutoResetEventOperation::await_suspend(std::coroutine_handle<> awaiter) noexcept {
    m_awaiter = awaiter;

    // Append to the queue, so waiters are resumed in FIFO order.
    if (m_event->m_waitersTail == nullptr) {
        m_event->m_waitersHead = this;
    } else {
        m_event->m_waitersTail->m_next = this;
    }
    m_event->m_waitersTail = this;
}
