///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

// Copied and modified from Lewis Baker's cppcoro

#pragma once

namespace Ichor::Detail {
    template <typename T>
    template <typename U> requires(!std::is_same_v<U, StartBehaviour>)
    inline AsyncGeneratorYieldOperation AsyncGeneratorPromise<T>::yield_value(value_type& value) noexcept(std::is_nothrow_constructible_v<T, T&&>) {
        static_assert(std::is_move_constructible_v<T>, "T needs to be move constructible");

#ifdef ICHOR_USE_HARDENING
        // check if currently on a different thread than creation time (which would be undefined behaviour)
        if(_dmAtTimeOfCreation != _local_dm) [[unlikely]] {
            std::terminate();
        }
#endif

        Ichor::Detail::_local_dm->getEventQueue().pushPrioritisedEvent<Ichor::ContinuableEvent>(_svcId, _priority, _id);

        INTERNAL_COROUTINE_DEBUG("yield_value<{}>(&) {} {}", typeName<T>(), _id, !!_consumerCoroutine);
        _currentValue.emplace(std::move(value));
        return internal_yield_value();
    }

    template <typename T>
    template <typename U> requires(!std::is_same_v<U, StartBehaviour>)
    inline AsyncGeneratorYieldOperation AsyncGeneratorPromise<T>::yield_value(value_type&& value) noexcept(std::is_nothrow_constructible_v<T, T&&>) {
        static_assert(std::is_move_constructible_v<T>, "T needs to be move constructible");

#ifdef ICHOR_USE_HARDENING
        // check if currently on a different thread than creation time (which would be undefined behaviour)
        if(_dmAtTimeOfCreation != _local_dm) [[unlikely]] {
            std::terminate();
        }
#endif

        Ichor::Detail::_local_dm->getEventQueue().pushPrioritisedEvent<Ichor::ContinuableEvent>(_svcId, _priority, _id);

        INTERNAL_COROUTINE_DEBUG("yield_value<{}>(&&) {} {}", typeName<T>(), _id, !!_consumerCoroutine);
        _currentValue.emplace(std::forward<T>(value));
        return internal_yield_value();
    }

    template <typename T>
    inline void AsyncGeneratorPromise<T>::return_value(value_type &value) noexcept(std::is_nothrow_constructible_v<T, T&&>) {
        static_assert(std::is_move_constructible_v<T>, "T needs to be move constructible");

#ifdef ICHOR_USE_HARDENING
        // check if currently on a different thread than creation time (which would be undefined behaviour)
        if(_dmAtTimeOfCreation != _local_dm) [[unlikely]] {
            std::terminate();
        }
#endif

        if constexpr(std::is_same_v<T, StartBehaviour>) {
            if(_hasSuspended.has_value() && *_hasSuspended) {
                [[maybe_unused]] auto evtId = Ichor::Detail::_local_dm->getEventQueue().pushPrioritisedEvent<Ichor::ContinuableStartEvent>(_svcId, _priority, _id);
                INTERNAL_COROUTINE_DEBUG("push continuable {} {}", _id, evtId);
            }
        }
        if constexpr(std::is_same_v<T, IchorBehaviour>) {
            if(_hasSuspended.has_value() && *_hasSuspended) {
                [[maybe_unused]] auto evtId = Ichor::Detail::_local_dm->getEventQueue().pushPrioritisedEvent<Ichor::ContinuableEvent>(_svcId, _priority, _id);
                INTERNAL_COROUTINE_DEBUG("push {} {}", _id, evtId);
            }
        }

        INTERNAL_COROUTINE_DEBUG("return_value<{}>(&) {} {}", typeName<T>(), _id, !!_consumerCoroutine);
        _currentValue.emplace(std::move(value));
    }

    template <typename T>
    inline void AsyncGeneratorPromise<T>::return_value(value_type &&value) noexcept(std::is_nothrow_constructible_v<T, T&&>) {
        static_assert(std::is_move_constructible_v<T>, "T needs to be move constructible");

#ifdef ICHOR_USE_HARDENING
        // check if currently on a different thread than creation time (which would be undefined behaviour)
        if(_dmAtTimeOfCreation != _local_dm) [[unlikely]] {
            std::terminate();
        }
#endif

        if constexpr(std::is_same_v<T, StartBehaviour>) {
            if(_hasSuspended.has_value() && *_hasSuspended) {
                [[maybe_unused]] auto evtId = Ichor::Detail::_local_dm->getEventQueue().pushPrioritisedEvent<Ichor::ContinuableStartEvent>(_svcId, _priority, _id);
                INTERNAL_COROUTINE_DEBUG("push continuable {} {}", _id, evtId);
            }
        }
        if constexpr(std::is_same_v<T, IchorBehaviour>) {
            if(_hasSuspended.has_value() && *_hasSuspended) {
                [[maybe_unused]] auto evtId = Ichor::Detail::_local_dm->getEventQueue().pushPrioritisedEvent<Ichor::ContinuableEvent>(_svcId, _priority, _id);
                INTERNAL_COROUTINE_DEBUG("push {}", _id, evtId);
            }
        }

        INTERNAL_COROUTINE_DEBUG("return_value<{}>(&&) {} {}", typeName<T>(), _id, !!_consumerCoroutine);
        _currentValue.emplace(std::forward<T>(value));
    }

    inline AsyncGeneratorYieldOperation AsyncGeneratorPromise<void>::yield_value(Empty) noexcept {
        INTERNAL_COROUTINE_DEBUG("yield_value<void> {} {}", _id, !!_consumerCoroutine);

#ifdef ICHOR_USE_HARDENING
        // check if currently on a different thread than creation time (which would be undefined behaviour)
        if(_dmAtTimeOfCreation != _local_dm) [[unlikely]] {
            std::terminate();
        }
#endif

        return internal_yield_value();
    }

    inline void AsyncGeneratorPromise<void>::return_void() noexcept {
        INTERNAL_COROUTINE_DEBUG("return_value<void> {} {}", _id, !!_consumerCoroutine);

#ifdef ICHOR_USE_HARDENING
        // check if currently on a different thread than creation time (which would be undefined behaviour)
        if(_dmAtTimeOfCreation != _local_dm) [[unlikely]] {
            std::terminate();
        }
#endif
    }

    ICHOR_COROUTINE_CONSTEXPR inline AsyncGenerator<void> AsyncGeneratorPromise<void>::get_return_object() noexcept {
        return AsyncGenerator<void>{ *this };
    }
}
