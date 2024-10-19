///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

// Copied and modified from Lewis Baker's cppcoro

#pragma once

#include <coroutine>
#include <cassert>
#include <functional>
#include <memory>
#include <tl/optional.h>
#include <utility>
#include <ichor/Enums.h>
#include <ichor/Common.h>
#include <ichor/stl/ReferenceCountedPointer.h>

namespace Ichor {
    template<typename T>
    class AsyncGenerator;

    class DependencyManager;

    struct Empty;
}

#ifdef ICHOR_ENABLE_INTERNAL_COROUTINE_DEBUGGING
#define ICHOR_COROUTINE_CONSTEXPR
#else
#define ICHOR_COROUTINE_CONSTEXPR constexpr
#endif

namespace Ichor::Detail {
    constinit thread_local extern DependencyManager *_local_dm;

    template<typename T>
    class AsyncGeneratorIterator;
    class AsyncGeneratorYieldOperation;
    class AsyncGeneratorAdvanceOperation;

    class AsyncGeneratorPromiseBase {
    public:

        AsyncGeneratorPromiseBase() noexcept
                : _state(state::value_ready_producer_suspended)

                , _exception(nullptr)
                , _id(_idCounter++)
        {
            // 0 has a special meaning. Maybe eventually turn this into an optional?
            // See TaskPromise f.e.
            if(_idCounter == 0) {
                _idCounter = 1;
            }
            // Other variables left intentionally uninitialised as they're
            // only referenced in certain states by which time they should
            // have been initialised.
        }
        virtual ~AsyncGeneratorPromiseBase() = default;

        AsyncGeneratorPromiseBase(const AsyncGeneratorPromiseBase& other) = delete;
        AsyncGeneratorPromiseBase& operator=(const AsyncGeneratorPromiseBase& other) = delete;

        ICHOR_COROUTINE_CONSTEXPR std::suspend_always initial_suspend() const noexcept {
            INTERNAL_COROUTINE_DEBUG("AsyncGeneratorPromiseBase::initial_suspend {} {}", _id, _state);
            return {};
        }

        constexpr AsyncGeneratorYieldOperation final_suspend() noexcept;

        constexpr void unhandled_exception() noexcept {
            // Don't bother capturing the exception if we have been cancelled
            // as there is no consumer that will see it.
            if (_state != state::cancelled)
            {
//                std::terminate();
                _exception = std::current_exception();
                std::rethrow_exception(std::move(_exception));
            }
        }

        /// Query if the generator has reached the end of the sequence.
        ///
        /// Only valid to call after resuming from an awaited advance operation.
        /// i.e. Either a begin() or iterator::operator++() operation.
        [[nodiscard]] constexpr virtual bool finished() const noexcept = 0;

        constexpr virtual void set_finished() noexcept = 0;

#if __cpp_constexpr >= 202207L
        constexpr
#endif
        void rethrow_if_unhandled_exception() {
            if (_exception)
            {
                std::rethrow_exception(std::move(_exception));
            }
        }

        /// Request that the generator cancel generation of new items.
        ///
        /// \return
        /// Returns true if the request was completed synchronously and the associated
        /// producer coroutine is now available to be destroyed. In which case the caller
        /// is expected to call destroy() on the coroutine_handle.
        /// Returns false if the producer coroutine was not at a suitable suspend-point.
        /// The coroutine will be destroyed when it next reaches a co_yield or co_return
        /// statement.
        ICHOR_COROUTINE_CONSTEXPR bool request_cancellation() noexcept {
            INTERNAL_COROUTINE_DEBUG("request_cancellation {}", _id);
            const auto previousState = std::exchange(_state, state::cancelled);

            // Not valid to destroy async_generator<T> object if consumer coroutine still suspended
            // in a co_await for next item.
            assert(previousState != state::value_not_ready_consumer_suspended);

            // A coroutine should only ever be cancelled once, from the destructor of the
            // owning async_generator<T> object.
            assert(previousState != state::cancelled);

            return previousState == state::value_ready_producer_suspended;
        }

        [[nodiscard]] constexpr uint64_t get_id() const noexcept {
            return _id;
        }

        [[nodiscard]] constexpr uint64_t get_priority() const noexcept {
            return _priority;
        }

        constexpr void set_priority(uint64_t priority) noexcept {
            _priority = priority;
        }

        [[nodiscard]] constexpr ServiceIdType get_service_id() const noexcept {
            return _svcId;
        }

        constexpr void set_service_id(ServiceIdType svcId) noexcept {
            _svcId = svcId;
        }

    protected:
        constexpr AsyncGeneratorYieldOperation internal_yield_value() noexcept;

    public:
        friend class AsyncGeneratorYieldOperation;
        friend class AsyncGeneratorAdvanceOperation;

        // Ichor forces everything to be on the same thread (or terminates the program)
        // Therefore, we don't need atomic state, like is used in cppcoro
        state _state;
        std::exception_ptr _exception;
        std::coroutine_handle<> _consumerCoroutine;
        uint64_t _id;
        uint64_t _priority{100}; // TODO: use INTERNAL_DEPENDENCY_EVENT_PRIORITY by refactoring headers
        ServiceIdType _svcId{};
#ifdef ICHOR_USE_HARDENING
        DependencyManager *_dmAtTimeOfCreation{_local_dm};
#endif
        tl::optional<bool> _hasSuspended{};

    private:
        static constinit thread_local uint64_t _idCounter;
    };


    class AsyncGeneratorYieldOperation final
    {
    public:

        constexpr AsyncGeneratorYieldOperation(AsyncGeneratorPromiseBase& promise, state initialState) noexcept
                : _promise(promise)
                , _initialState(initialState)
        {
        }

        ICHOR_COROUTINE_CONSTEXPR bool await_ready() const noexcept {
            INTERNAL_COROUTINE_DEBUG("AsyncGeneratorYieldOperation::await_ready {} {}", _initialState, _promise._id);
            return _initialState == state::value_not_ready_consumer_suspended;
        }

        constexpr bool await_suspend(std::coroutine_handle<> producer) noexcept;

        constexpr void await_resume() noexcept {
        }

    private:
        AsyncGeneratorPromiseBase& _promise;
        state _initialState;
    };

    template<typename T>
    class AsyncGeneratorPromise final : public AsyncGeneratorPromiseBase
    {
        using value_type = std::remove_reference_t<T>;

    public:
        AsyncGeneratorPromise() noexcept : _destroyed(new bool(false)) {
            INTERNAL_COROUTINE_DEBUG("AsyncGeneratorPromise<{}>() {}", typeName<T>(), *_destroyed);
        }
        ~AsyncGeneratorPromise() final {
            INTERNAL_COROUTINE_DEBUG("~AsyncGeneratorPromise<{}>()", typeName<T>());
            *_destroyed = true;
        };

        constexpr AsyncGenerator<T> get_return_object() noexcept {
            return AsyncGenerator<T>{ *this };
        }

        template <typename U = T> requires(!std::is_same_v<U, StartBehaviour>)
        AsyncGeneratorYieldOperation yield_value(value_type& value) noexcept(std::is_nothrow_constructible_v<T, T&&>);

        template <typename U = T> requires(!std::is_same_v<U, StartBehaviour>)
        AsyncGeneratorYieldOperation yield_value(value_type&& value) noexcept(std::is_nothrow_constructible_v<T, T&&>);

        void return_value(value_type &value) noexcept(std::is_nothrow_constructible_v<T, T&&>);

        void return_value(value_type &&value) noexcept(std::is_nothrow_constructible_v<T, T&&>);

        ICHOR_COROUTINE_CONSTEXPR T& value() noexcept {
            INTERNAL_COROUTINE_DEBUG("request value {}, {}", _id, _currentValue.has_value());
            return _currentValue.value();
        }

        [[nodiscard]] constexpr bool finished() const noexcept final {
            return _finished;
        }

        ICHOR_COROUTINE_CONSTEXPR ReferenceCountedPointer<bool>& get_destroyed() noexcept  {
            INTERNAL_COROUTINE_DEBUG("AsyncGeneratorPromise<{}>::get_destroyed {}, {}", typeName<T>(), _id, *_destroyed);
            return _destroyed;
        }

    private:
        ICHOR_COROUTINE_CONSTEXPR void set_finished() noexcept final {
            INTERNAL_COROUTINE_DEBUG("set_finished {} {}", _id, typeName<T>());

#ifdef ICHOR_USE_HARDENING
            if(!_currentValue) [[unlikely]] {
                if(_exception) {
                    std::rethrow_exception(std::move(_exception));
                } else {
                    std::terminate();
                }
            }
#endif
            _finished = true;
        }

        tl::optional<T> _currentValue{};
        bool _finished{};
        ReferenceCountedPointer<bool> _destroyed;
    };

    template<>
    class AsyncGeneratorPromise<void> final : public AsyncGeneratorPromiseBase
    {
    public:
        AsyncGeneratorPromise() noexcept : _destroyed(new bool(false)) {
            INTERNAL_COROUTINE_DEBUG("AsyncGeneratorPromise<>()");
        }
        ~AsyncGeneratorPromise() final {
            INTERNAL_COROUTINE_DEBUG("~AsyncGeneratorPromise<>()");
            *_destroyed = true;
        };

        constexpr AsyncGenerator<void> get_return_object() noexcept;

        AsyncGeneratorYieldOperation yield_value(Ichor::Empty) noexcept;

        void return_void() noexcept;

        [[nodiscard]] constexpr bool finished() const noexcept final {
            return _finished;
        }

        constexpr ReferenceCountedPointer<bool>& get_destroyed() noexcept  {
            return _destroyed;
        }

    private:
        ICHOR_COROUTINE_CONSTEXPR void set_finished() noexcept final {
            INTERNAL_COROUTINE_DEBUG("set_finished {}", _id);
            _finished = true;
        }

        bool _finished{};
        ReferenceCountedPointer<bool> _destroyed;
    };
}

#include <ichor/events/ContinuableEvent.h>
