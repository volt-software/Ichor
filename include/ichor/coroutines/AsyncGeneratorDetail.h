///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

// Copied and modified from Lewis Baker's cppcoro

#pragma once

#include <ichor/Assert.h>
#include <ichor/DependencyManager.h>

namespace Ichor::v1::Detail {
    inline void AsyncGeneratorPromiseBase::terminate_if_on_wrong_thread() const noexcept {
        // check if currently on a different thread than creation time (which would be undefined behaviour)
        ICHOR_ASSERT(_dmAtTimeOfCreation == _local_dm, "generator {} used on another thread than the one it was created on", get_id());
    }

    template <typename T>
    inline void AsyncGeneratorPromise<T>::push_continuable_if_suspended() {
        // If the coroutine suspended at some point, the DependencyManager is waiting on an event to continue processing it.
        if(!_hasSuspended.has_value() || !*_hasSuspended) {
            return;
        }

        if constexpr(std::is_same_v<T, StartBehaviour>) {
            auto svcId = get_service_id();
            [[maybe_unused]] auto evtId = Ichor::v1::Detail::_local_dm->getEventQueue().pushPrioritisedEvent<Ichor::ContinuableStartEvent>(svcId, _priority, _id);
            INTERNAL_COROUTINE_DEBUG("push continuable {} {}", _id, evtId);
        }
        if constexpr(std::is_same_v<T, IchorBehaviour>) {
            auto svcId = get_service_id();
            [[maybe_unused]] auto evtId = Ichor::v1::Detail::_local_dm->getEventQueue().pushPrioritisedEvent<Ichor::ContinuableEvent>(svcId, _priority, _id);
            INTERNAL_COROUTINE_DEBUG("push {} {}", _id, evtId);
        }
    }

    template <typename T>
    template <typename U> requires(!std::is_same_v<U, StartBehaviour> && std::is_copy_constructible_v<U>)
    inline AsyncGeneratorYieldOperation AsyncGeneratorPromise<T>::yield_value(value_type const & value) {
        terminate_if_on_wrong_thread();

        auto svcId = get_service_id();
        Ichor::v1::Detail::_local_dm->getEventQueue().pushPrioritisedEvent<Ichor::ContinuableEvent>(svcId, _priority, _id);

        INTERNAL_COROUTINE_DEBUG("yield_value<{}>(const &) {} {}", typeName<T>(), _id, !!_consumerCoroutine);
        _currentValue.emplace(value);
        return internal_yield_value();
    }

    template <typename T>
    template <typename U> requires(!std::is_same_v<U, StartBehaviour>)
    inline AsyncGeneratorYieldOperation AsyncGeneratorPromise<T>::yield_value(value_type&& value) {
        static_assert(std::is_move_constructible_v<T>, "T needs to be move constructible");
        terminate_if_on_wrong_thread();

        auto svcId = get_service_id();
        // TODO refactor ContinuableEvent for stop events into its own event type, so that checking _pendingStopsDueToCoroutine doesn't happen for coroutines that have nothing to do with Ichor specific things
        // This probable requires introducing a new enum like IchorBehaviour, but specifically for _pendingStopsDueToCoroutine related things
        Ichor::v1::Detail::_local_dm->getEventQueue().pushPrioritisedEvent<Ichor::ContinuableEvent>(svcId, _priority, _id);

        INTERNAL_COROUTINE_DEBUG("yield_value<{}>(&&) {} {}", typeName<T>(), _id, !!_consumerCoroutine);
        _currentValue.emplace(std::move(value));
        return internal_yield_value();
    }

    template <typename T>
    inline void AsyncGeneratorPromise<T>::return_value(value_type const &value) requires std::is_copy_constructible_v<T> {
        terminate_if_on_wrong_thread();
        push_continuable_if_suspended();

        INTERNAL_COROUTINE_DEBUG("return_value<{}>(const &) {} {}", typeName<T>(), _id, !!_consumerCoroutine);
        _currentValue.emplace(value);
    }

    template <typename T>
    inline void AsyncGeneratorPromise<T>::return_value(value_type &&value) {
        static_assert(std::is_move_constructible_v<T>, "T needs to be move constructible");
        terminate_if_on_wrong_thread();
        push_continuable_if_suspended();

        INTERNAL_COROUTINE_DEBUG("return_value<{}>(&&) {} {}", typeName<T>(), _id, !!_consumerCoroutine);
        _currentValue.emplace(std::move(value));
    }

    inline AsyncGeneratorYieldOperation AsyncGeneratorPromise<void>::yield_value(Empty) noexcept {
        INTERNAL_COROUTINE_DEBUG("yield_value<void> {} {}", _id, !!_consumerCoroutine);
        terminate_if_on_wrong_thread();

        return internal_yield_value();
    }

    inline void AsyncGeneratorPromise<void>::return_void() noexcept {
        INTERNAL_COROUTINE_DEBUG("return_value<void> {} {}", _id, !!_consumerCoroutine);
        terminate_if_on_wrong_thread();
    }

    ICHOR_COROUTINE_CONSTEXPR inline AsyncGenerator<void> AsyncGeneratorPromise<void>::get_return_object() noexcept {
        return AsyncGenerator<void>{ *this };
    }
}
