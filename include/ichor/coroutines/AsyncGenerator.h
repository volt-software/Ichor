///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

// Copied and modified from Lewis Baker's cppcoro

#pragma once

#include <coroutine>
#include <atomic>
#include <cassert>
#include <functional>
#include <ichor/Enums.h>
#include <ichor/coroutines/IGenerator.h>
#include <ichor/coroutines/AsyncGeneratorPromiseBase.h>

namespace Ichor {
    template<typename T>
    class AsyncGenerator;

    namespace Detail {
        class AsyncGeneratorAdvanceOperation {
        protected:
            AsyncGeneratorAdvanceOperation(std::nullptr_t) noexcept
                : _promise(nullptr)
                , _producerCoroutine(nullptr)
            {}

            AsyncGeneratorAdvanceOperation(
                AsyncGeneratorPromiseBase& promise,
                std::coroutine_handle<> producerCoroutine) noexcept
                : _promise(std::addressof(promise))
                , _producerCoroutine(producerCoroutine)
            {
                state initialState = promise._state.load(std::memory_order_acquire);
                if (initialState == state::value_ready_producer_suspended) {
                    // Can use relaxed memory order here as we will be resuming the producer
                    // on the same thread.
                    promise._state.store(state::value_not_ready_consumer_active, std::memory_order_relaxed);

                    producerCoroutine.resume();

                    // Need to use acquire memory order here since it's possible that the
                    // coroutine may have transferred execution to another thread and
                    // completed on that other thread before the call to resume() returns.
                    initialState = promise._state.load(std::memory_order_acquire);
                }

                _initialState = initialState;
            }

        public:
            bool await_ready() const noexcept {
                return _initialState == state::value_ready_producer_suspended;
            }

            bool await_suspend(std::coroutine_handle<> consumerCoroutine) noexcept {
                _promise->_consumerCoroutine = consumerCoroutine;

                auto currentState = _initialState;
                if (currentState == state::value_ready_producer_active) {
                    // A potential race between whether consumer or producer coroutine
                    // suspends first. Resolve the race using a compare-exchange.
                    if (_promise->_state.compare_exchange_strong(
                        currentState,
                        state::value_not_ready_consumer_suspended,
                        std::memory_order_release,
                        std::memory_order_acquire)) {
                        return true;
                    }

                    assert(currentState == state::value_ready_producer_suspended);

                    _promise->_state.store(state::value_not_ready_consumer_active, std::memory_order_relaxed);

                    _producerCoroutine.resume();

                    currentState = _promise->_state.load(std::memory_order_acquire);
                    if (currentState == state::value_ready_producer_suspended) {
                        // Producer coroutine produced a value synchronously.
                        return false;
                    }
                }

                assert(currentState == state::value_not_ready_consumer_active);

                // Try to suspend consumer coroutine, transitioning to value_not_ready_consumer_suspended.
                // This could be racing with producer making the next value available and suspending
                // (transition to value_ready_producer_suspended) so we use compare_exchange to decide who
                // wins the race.
                // If compare_exchange succeeds then consumer suspended (and we return true).
                // If it fails then producer yielded next value and suspended and we can return
                // synchronously without suspended (ie. return false).
                return _promise->_state.compare_exchange_strong(
                    currentState,
                    state::value_not_ready_consumer_suspended,
                    std::memory_order_release,
                    std::memory_order_acquire);
            }

        protected:
            AsyncGeneratorPromiseBase* _promise;
            std::coroutine_handle<> _producerCoroutine;

        public:
            state _initialState;
        };

        template<typename T>
        class AsyncGeneratorIncrementOperation final : public AsyncGeneratorAdvanceOperation {
        public:
            AsyncGeneratorIncrementOperation(AsyncGeneratorIterator<T>& iterator) noexcept
                : AsyncGeneratorAdvanceOperation(iterator._coroutine.promise(), iterator._coroutine)
                , _iterator(iterator)
            {}

            AsyncGeneratorIterator<T>& await_resume();

        private:
            AsyncGeneratorIterator<T>& _iterator;
        };

        template<typename T>
        class AsyncGeneratorIterator final {
            using promise_type = AsyncGeneratorPromise<T>;
            using handle_type = std::coroutine_handle<promise_type>;

        public:

            using iterator_category = std::input_iterator_tag;
            // Not sure what type should be used for difference_type as we don't
            // allow calculating difference between two iterators.
            using difference_type = std::ptrdiff_t;
            using value_type = std::remove_reference_t<T>;
            using reference = std::add_lvalue_reference_t<T>;
            using pointer = std::add_pointer_t<value_type>;

            AsyncGeneratorIterator(std::nullptr_t) noexcept
                : _coroutine(nullptr)
            {}

            AsyncGeneratorIterator(handle_type coroutine) noexcept
                : _coroutine(coroutine)
            {}

            AsyncGeneratorIncrementOperation<T> operator++() noexcept {
                return AsyncGeneratorIncrementOperation<T>{ *this };
            }

            reference operator*() const noexcept {
                return _coroutine.promise().value();
            }

            bool operator==(const AsyncGeneratorIterator& other) const noexcept {
                return _coroutine == other._coroutine;
            }

            bool operator!=(const AsyncGeneratorIterator& other) const noexcept {
                return !(*this == other);
            }

        private:
            friend class AsyncGeneratorIncrementOperation<T>;
            handle_type _coroutine;
        };

        template<typename T>
        AsyncGeneratorIterator<T>& AsyncGeneratorIncrementOperation<T>::await_resume() {
            if (_promise->finished()) {
                // Update iterator to end()
                _iterator = AsyncGeneratorIterator<T>{ nullptr };
                _promise->rethrow_if_unhandled_exception();
            }

            return _iterator;
        }

        template<typename T>
        class AsyncGeneratorBeginOperation final : public AsyncGeneratorAdvanceOperation, public IAsyncGeneratorBeginOperation
        {
            using promise_type = AsyncGeneratorPromise<T>;
            using handle_type = std::coroutine_handle<promise_type>;

        public:

            AsyncGeneratorBeginOperation(std::nullptr_t) noexcept
                : AsyncGeneratorAdvanceOperation(nullptr)
            {}

            AsyncGeneratorBeginOperation(handle_type producerCoroutine) noexcept
                : AsyncGeneratorAdvanceOperation(producerCoroutine.promise(), producerCoroutine)
            {}

            ~AsyncGeneratorBeginOperation() final = default;

            bool await_ready() const noexcept {
                return _promise == nullptr || AsyncGeneratorAdvanceOperation::await_ready();
            }

            [[nodiscard]] bool get_finished() const noexcept final {
                return _promise == nullptr || _promise->finished();
            }

            [[nodiscard]] state get_op_state() const noexcept final {
                return _initialState;
            }

            [[nodiscard]] state get_promise_state() const noexcept final {
                return _promise->_state;
            }

            [[nodiscard]] bool promise_is_null() const noexcept {
                return _promise == nullptr;
            }

            [[nodiscard]] uint64_t get_promise_id() const noexcept final {
                if(_promise == nullptr) {
                    return 0;
                }

                return _promise->get_id();
            }

            AsyncGeneratorIterator<T> await_resume()
            {
                if (_promise == nullptr) {
                    // Called begin() on the empty generator.
                    return AsyncGeneratorIterator<T>{ nullptr };
                } else if (_promise->finished()) {
                    _promise->rethrow_if_unhandled_exception();
                }

                return AsyncGeneratorIterator<T>{
                    handle_type::from_promise(*static_cast<promise_type*>(_promise))
                };
            }
        };

        inline bool AsyncGeneratorYieldOperation::await_suspend(std::coroutine_handle<> producer) noexcept {
            state currentState = _initialState;
            if (currentState == state::value_not_ready_consumer_active) {
                bool producerSuspended = _promise._state.compare_exchange_strong(
                    currentState,
                    state::value_ready_producer_suspended,
                    std::memory_order_release,
                    std::memory_order_acquire);
                if (producerSuspended) {
                    return true;
                }

                if (currentState == state::value_not_ready_consumer_suspended) {
                    // Can get away with using relaxed memory semantics here since we're
                    // resuming the consumer on the current thread.
                    _promise._state.store(state::value_ready_producer_active, std::memory_order_relaxed);

                    _promise._consumerCoroutine.resume();

                    // The consumer might have asked for another value before returning, in which case
                    // it'll transition to value_not_ready_consumer_suspended and we can return without
                    // suspending, otherwise we should try to suspend the producer, in which case the
                    // consumer will wake us up again when it wants the next value.
                    //
                    // Need to use acquire semantics here since it's possible that the consumer might
                    // have asked for the next value on a different thread which executed concurrently
                    // with the call to _consumerCoro.resume() above.
                    currentState = _promise._state.load(std::memory_order_acquire);
                    if (currentState == state::value_not_ready_consumer_suspended) {
                        return false;
                    }
                }
            }

            // By this point the consumer has been resumed if required and is now active.

            if (currentState == state::value_ready_producer_active) {
                // Try to suspend the producer.
                // If we failed to suspend then it's either because the consumer destructed, transitioning
                // the state to cancelled, or requested the next item, transitioning the state to value_not_ready_consumer_suspended.
                const bool suspendedProducer = _promise._state.compare_exchange_strong(
                    currentState,
                    state::value_ready_producer_suspended,
                    std::memory_order_release,
                    std::memory_order_acquire);
                if (suspendedProducer) {
                    return true;
                }

                if (currentState == state::value_not_ready_consumer_suspended) {
                    // Consumer has asked for the next value.
                    return false;
                }
            }

            assert(currentState == state::cancelled);

            // async_generator object has been destroyed and we're now at a
            // co_yield/co_return suspension point so we can just destroy
            // the coroutine.
            producer.destroy();

            return true;
        }

        inline AsyncGeneratorYieldOperation AsyncGeneratorPromiseBase::final_suspend() noexcept {
            set_finished();
            return internal_yield_value();
        }

        inline AsyncGeneratorYieldOperation AsyncGeneratorPromiseBase::internal_yield_value() noexcept {
            state currentState = _state.load(std::memory_order_acquire);
            assert(currentState != state::value_ready_producer_active);
            assert(currentState != state::value_ready_producer_suspended);

            if (currentState == state::value_not_ready_consumer_suspended) {
                // Only need relaxed memory order since we're resuming the
                // consumer on the same thread.
                _state.store(state::value_ready_producer_active, std::memory_order_relaxed);

                // Resume the consumer.
                // It might ask for another value before returning, in which case it'll
                // transition to value_not_ready_consumer_suspended and we can return from
                // yield_value without suspending, otherwise we should try to suspend
                // the producer in which case the consumer will wake us up again
                // when it wants the next value.
                _consumerCoroutine.resume();

                // Need to use acquire semantics here since it's possible that the
                // consumer might have asked for the next value on a different thread
                // which executed concurrently with the call to _consumerCoro on the
                // current thread above.
                currentState = _state.load(std::memory_order_acquire);
            }

            return AsyncGeneratorYieldOperation{ *this, currentState };
        }
    }

    struct Empty{};
    constexpr static Empty empty = {};

    template<typename T>
    class AsyncGenerator final : public IGenerator {
    public:
        using promise_type = Detail::AsyncGeneratorPromise<T>;
        using iterator = Detail::AsyncGeneratorIterator<T>;

        AsyncGenerator() noexcept
            : IGenerator(), _coroutine(nullptr), _destroyed()
        {
            INTERNAL_DEBUG("AsyncGenerator()");
        }

        explicit AsyncGenerator(promise_type& promise) noexcept
            : IGenerator(), _coroutine(std::coroutine_handle<promise_type>::from_promise(promise)), _destroyed(promise.get_destroyed())
        {
            INTERNAL_DEBUG("AsyncGenerator(promise_type& promise) {}", _coroutine.promise().get_id());
        }

        AsyncGenerator(AsyncGenerator&& other) noexcept
            : IGenerator(), _coroutine(other._coroutine), _destroyed(other._destroyed) {
            INTERNAL_DEBUG("AsyncGenerator(AsyncGenerator&& other) {}", _coroutine.promise().get_id());
            other._coroutine = nullptr;
        }

        ~AsyncGenerator() final {
            INTERNAL_DEBUG("~AsyncGenerator() {} {} {}", *_destroyed, _coroutine == nullptr, !(*_destroyed) && _coroutine != nullptr ? _coroutine.promise().get_id() : 0);
            if (!(*_destroyed) && _coroutine) {
                if (_coroutine.promise().request_cancellation()) {
                    _coroutine.destroy();
                    *_destroyed = true;
                }
            }
        }

        AsyncGenerator& operator=(AsyncGenerator&& other) noexcept {
            INTERNAL_DEBUG("operator=(AsyncGenerator&& other) {}", other._id);
            AsyncGenerator temp(std::move(other));
            swap(temp);
            return *this;
        }

        AsyncGenerator(const AsyncGenerator&) = delete;
        AsyncGenerator& operator=(const AsyncGenerator&) = delete;

        [[nodiscard]] auto begin() noexcept {
            if ((*_destroyed) || !_coroutine) {
                return Detail::AsyncGeneratorBeginOperation<T>{ nullptr };
            }

            return Detail::AsyncGeneratorBeginOperation<T>{ _coroutine };
        }

        [[nodiscard]] std::unique_ptr<IAsyncGeneratorBeginOperation> begin_interface() noexcept final {
            if ((*_destroyed) || !_coroutine) {
                return std::make_unique<Detail::AsyncGeneratorBeginOperation<T>>(nullptr);
            }

            return std::make_unique<Detail::AsyncGeneratorBeginOperation<T>>(_coroutine);
        }

        auto end() noexcept {
            return iterator{ nullptr };
        }

        [[nodiscard]] bool done() const noexcept final {
//            std::string _promise_id = !(*_destroyed) ? _coroutine != nullptr ? std::to_string(_coroutine.promise().get_id()) : "NULLED" : "DESTROYED";
//            std::string_view _nullptr = !(*_destroyed) ? _coroutine == nullptr ? "null" : "non-null" : "DESTROYED";
//            std::string_view _done = !(*_destroyed) ? _coroutine != nullptr ? _coroutine.done() ? "done" : "not done" : "NULLED" : "DESTROYED";
//            INTERNAL_DEBUG("done() {} {} {} {}"
//                         , _promise_id
//                         , *_destroyed
//                         , _nullptr
//                         , _done);
            return (*_destroyed) || _coroutine == nullptr || _coroutine.done();
        }

        void swap(AsyncGenerator& other) noexcept {
            using std::swap;
            swap(_coroutine, other._coroutine);
            swap(_destroyed, other._destroyed);
        }

    private:
        std::coroutine_handle<promise_type> _coroutine;
        std::shared_ptr<bool> _destroyed;
    };

    template<typename T>
    void swap(AsyncGenerator<T>& a, AsyncGenerator<T>& b) noexcept {
        a.swap(b);
    }

    template<typename FUNC, typename T>
    AsyncGenerator<std::invoke_result_t<FUNC&, decltype(*std::declval<typename AsyncGenerator<T>::iterator&>())>> fmap(FUNC func, AsyncGenerator<T> source) {
        static_assert(
            !std::is_reference_v<FUNC>,
            "Passing by reference to AsyncGenerator<T> coroutine is unsafe. "
            "Use std::ref or std::cref to explicitly pass by reference.");

        // Explicitly hand-coding the loop here rather than using range-based
        // for loop since it's difficult to std::forward<???> the value of a
        // range-based for-loop, preserving the value category of operator*
        // return-value.
        auto it = co_await source.begin();
        const auto itEnd = source.end();
        while (it != itEnd) {
            co_yield std::invoke(func, *it);
            (void)co_await ++it;
        }
    }
}

namespace Ichor::Detail {
    template <typename T>
    inline AsyncGeneratorYieldOperation AsyncGeneratorPromise<T>::yield_value(value_type& value) noexcept(std::is_nothrow_constructible_v<T, T&&>) {
        static_assert(std::is_move_constructible_v<T>, "T needs to be move constructible");
        INTERNAL_DEBUG("yield_value {}", _id);
        _currentValue.emplace(std::move(value));
        Ichor::Detail::_local_dm->pushPrioritisedEvent<Ichor::ContinuableEvent>(0, INTERNAL_DEPENDENCY_EVENT_PRIORITY, _id);
        return internal_yield_value();
    }

    template <typename T>
    inline AsyncGeneratorYieldOperation AsyncGeneratorPromise<T>::yield_value(value_type&& value) noexcept(std::is_nothrow_constructible_v<T, T&&>) {
        static_assert(std::is_move_constructible_v<T>, "T needs to be move constructible");
        INTERNAL_DEBUG("yield_value {}", _id);
        _currentValue.emplace(std::forward<T>(value));
        Ichor::Detail::_local_dm->pushPrioritisedEvent<Ichor::ContinuableEvent>(0, INTERNAL_DEPENDENCY_EVENT_PRIORITY, _id);
        return internal_yield_value();
    }

    template <typename T>
    inline void AsyncGeneratorPromise<T>::return_value(value_type &value) noexcept(std::is_nothrow_constructible_v<T, T&&>) {
        static_assert(std::is_move_constructible_v<T>, "T needs to be move constructible");
        INTERNAL_DEBUG("return_value {}", _id);
        _currentValue.emplace(std::move(value));
        Ichor::Detail::_local_dm->pushPrioritisedEvent<Ichor::ContinuableEvent>(0, INTERNAL_DEPENDENCY_EVENT_PRIORITY, _id);
    }

    template <typename T>
    inline void AsyncGeneratorPromise<T>::return_value(value_type &&value) noexcept(std::is_nothrow_constructible_v<T, T&&>) {
        static_assert(std::is_move_constructible_v<T>, "T needs to be move constructible");
        INTERNAL_DEBUG("return_value {}", _id);
        _currentValue.emplace(std::forward<T>(value));
        Ichor::Detail::_local_dm->pushPrioritisedEvent<Ichor::ContinuableEvent>(0, INTERNAL_DEPENDENCY_EVENT_PRIORITY, _id);
    }

    inline AsyncGeneratorYieldOperation AsyncGeneratorPromise<void>::yield_value(Empty) noexcept {
        INTERNAL_DEBUG("yield_value {}", _id);;
        Ichor::Detail::_local_dm->pushPrioritisedEvent<Ichor::ContinuableEvent>(0, INTERNAL_DEPENDENCY_EVENT_PRIORITY, _id);
        return internal_yield_value();
    }

    inline void AsyncGeneratorPromise<void>::return_void() noexcept {
        INTERNAL_DEBUG("return_value {}", _id);
        Ichor::Detail::_local_dm->pushPrioritisedEvent<Ichor::ContinuableEvent>(0, INTERNAL_DEPENDENCY_EVENT_PRIORITY, _id);
    }

    inline AsyncGenerator<void> AsyncGeneratorPromise<void>::get_return_object() noexcept {
        return AsyncGenerator<void>{ *this };
    }
}
