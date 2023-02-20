///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

// Copied and modified from Lewis Baker's cppcoro

#pragma once

#include <coroutine>
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
            {
            }

            AsyncGeneratorAdvanceOperation(
                AsyncGeneratorPromiseBase& promise,
                std::coroutine_handle<> producerCoroutine) noexcept
                : _promise(std::addressof(promise))
                , _producerCoroutine(producerCoroutine)
            {
                state initialState = promise._state;
                if (initialState == state::value_ready_producer_suspended) {
                    promise._state = state::value_not_ready_consumer_active;

                    producerCoroutine.resume();

                    initialState = promise._state;
                }

                _initialState = initialState;
            }

        public:
            bool await_ready() const noexcept {
                INTERNAL_COROUTINE_DEBUG("AsyncGeneratorAdvanceOperation::await_ready {} {}", _initialState, _promise->_id);
                return _initialState == state::value_ready_producer_suspended;
            }

            bool await_suspend(std::coroutine_handle<> consumerCoroutine) noexcept {
                INTERNAL_COROUTINE_DEBUG("AsyncGeneratorAdvanceOperation::await_suspend {} {}", _promise->_id, _promise->finished());
                _promise->_consumerCoroutine = consumerCoroutine;
                if(!_promise->_hasSuspended.has_value()) {
                    _promise->_hasSuspended = false;
                }

                auto currentState = _initialState;
                if (currentState == state::value_ready_producer_active) {
                    if (_promise->_state == currentState) {
                        _promise->_state = state::value_not_ready_consumer_suspended;
                        if(!_promise->finished()) {
                            _promise->_hasSuspended = true;
                        }
                        INTERNAL_COROUTINE_DEBUG("AsyncGeneratorAdvanceOperation::await_suspend1");
                        return true;
                    }
                    currentState = _promise->_state;

                    assert(currentState == state::value_ready_producer_suspended);

                    _promise->_state = state::value_not_ready_consumer_active;

                    _producerCoroutine.resume();

                    currentState = _promise->_state;
                    if (currentState == state::value_ready_producer_suspended) {
                        // Producer coroutine produced a value synchronously.
                        INTERNAL_COROUTINE_DEBUG("AsyncGeneratorAdvanceOperation::await_suspend2");
                        return false;
                    }
                }

                assert(currentState == state::value_not_ready_consumer_active);

                if(_promise->_state == currentState) {
                    _promise->_state = state::value_not_ready_consumer_suspended;
                    _promise->_hasSuspended = true;
                    INTERNAL_COROUTINE_DEBUG("AsyncGeneratorAdvanceOperation::await_suspend3");
                    return true;
                }
                INTERNAL_COROUTINE_DEBUG("AsyncGeneratorAdvanceOperation::await_suspend4");
                return false;
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
            {
            }

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
            INTERNAL_COROUTINE_DEBUG("AsyncGeneratorIncrementOperation<{}>::await_resume {}", typeName<T>(), _promise->_id);
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
            {
            }

            AsyncGeneratorBeginOperation(handle_type producerCoroutine) noexcept
                : AsyncGeneratorAdvanceOperation(producerCoroutine.promise(), producerCoroutine)
            {
            }

            ~AsyncGeneratorBeginOperation() final = default;

            bool await_ready() const noexcept {
                INTERNAL_COROUTINE_DEBUG("AsyncGeneratorBeginOperation<{}>::await_ready {}", typeName<T>(), _promise == nullptr ? 0u : _promise->_id);
                return _promise == nullptr || AsyncGeneratorAdvanceOperation::await_ready();
            }

            [[nodiscard]] bool get_finished() const noexcept final {
                return _promise == nullptr || _promise->finished();
            }

            /// Whenever a consumer/producer routine suspends, the generator promise sets the value.
            /// If the coroutine never ends up in await_suspend, f.e. because of co_await'ing on another coroutine immediately,
            /// suspended is in an un-set state and is considered as suspended.
            /// \return true if suspended or never engaged
            [[nodiscard]] bool get_has_suspended() const noexcept final {
                return _promise != nullptr && (!_promise->_hasSuspended.has_value() || *_promise->_hasSuspended);
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

            template <typename U = T> requires (!std::is_same_v<U, void>)
            [[nodiscard]] U& get_value() noexcept {
                auto prom = std::coroutine_handle<promise_type>::from_promise(*static_cast<promise_type*>(_promise));
                return prom.promise().value();
            }

            AsyncGeneratorIterator<T> await_resume() {
                if (_promise == nullptr) {
                    INTERNAL_COROUTINE_DEBUG("AsyncGeneratorIterator<{}>::await_resume NULL", typeName<T>());
                    // Called begin() on the empty generator.
                    return AsyncGeneratorIterator<T>{ nullptr };
                } else if (_promise->finished()) {
                    _promise->rethrow_if_unhandled_exception();
                }
                INTERNAL_COROUTINE_DEBUG("AsyncGeneratorIterator<{}>::await_resume {}", typeName<T>(), _promise->_id);

                return AsyncGeneratorIterator<T>{
                    handle_type::from_promise(*static_cast<promise_type*>(_promise))
                };
            }
        };

        inline bool AsyncGeneratorYieldOperation::await_suspend(std::coroutine_handle<> producer) noexcept {
            INTERNAL_COROUTINE_DEBUG("AsyncGeneratorYieldOperation::await_suspend {} {}", _promise._id, _promise.finished());
            state currentState = _initialState;
            if(!_promise._hasSuspended.has_value()) {
                _promise._hasSuspended = false;
            }

            if (currentState == state::value_not_ready_consumer_active) {
                bool producerSuspended{};
                if(_promise._state == currentState) {
                    _promise._state = state::value_ready_producer_suspended;
                    producerSuspended = true;
                }
                if (producerSuspended) {
                    INTERNAL_COROUTINE_DEBUG("AsyncGeneratorYieldOperation::await_suspend1");
                    if(!_promise.finished()) {
                        _promise._hasSuspended = true;
                    }
                    return true;
                }

                std::terminate();
            }

            // By this point the consumer has been resumed if required and is now active.

            if (currentState == state::value_ready_producer_active) {
                // Try to suspend the producer.
                // If we failed to suspend then it's either because the consumer destructed, transitioning
                // the state to cancelled, or requested the next item, transitioning the state to value_not_ready_consumer_suspended.
                bool suspendedProducer{};
                if(_promise._state == currentState) {
                    _promise._state = state::value_ready_producer_suspended;
                    suspendedProducer = true;
                }
                if (suspendedProducer) {
                    _promise._hasSuspended = true;
                    INTERNAL_COROUTINE_DEBUG("AsyncGeneratorYieldOperation::await_suspend3");
                    return true;
                }

                std::terminate();
            }

            assert(currentState == state::cancelled);

            // async_generator object has been destroyed and we're now at a
            // co_yield/co_return suspension point so we can just destroy
            // the coroutine.
            producer.destroy();

            INTERNAL_COROUTINE_DEBUG("AsyncGeneratorYieldOperation::await_suspend5");
            return true;
        }

        inline AsyncGeneratorYieldOperation AsyncGeneratorPromiseBase::final_suspend() noexcept {
            INTERNAL_COROUTINE_DEBUG("AsyncGeneratorPromiseBase::final_suspend {}", _id);
            set_finished();
            return internal_yield_value();
        }

        inline AsyncGeneratorYieldOperation AsyncGeneratorPromiseBase::internal_yield_value() noexcept {
            assert(_state != state::value_ready_producer_active);
            assert(_state != state::value_ready_producer_suspended);

            if (_state == state::value_not_ready_consumer_suspended) {
                _state = state::value_ready_producer_active;

                // Resume the consumer.
                // It might ask for another value before returning, in which case it'll
                // transition to value_not_ready_consumer_suspended and we can return from
                // yield_value without suspending, otherwise we should try to suspend
                // the producer in which case the consumer will wake us up again
                // when it wants the next value.
                _consumerCoroutine.resume();
            }

            return AsyncGeneratorYieldOperation{ *this, _state };
        }
    }

    template<typename T>
    class AsyncGenerator final : public IGenerator {
    public:
        using promise_type = Detail::AsyncGeneratorPromise<T>;
        using iterator = Detail::AsyncGeneratorIterator<T>;

        AsyncGenerator() noexcept
            : IGenerator(), _coroutine(nullptr), _destroyed()
        {
            INTERNAL_COROUTINE_DEBUG("AsyncGenerator<{}>()", typeName<T>());
        }

        explicit AsyncGenerator(promise_type& promise) noexcept
            : IGenerator(), _coroutine(std::coroutine_handle<promise_type>::from_promise(promise)), _destroyed(promise.get_destroyed())
        {
            INTERNAL_COROUTINE_DEBUG("AsyncGenerator<{}>(promise_type& promise) {}", typeName<T>(), _coroutine.promise().get_id());
        }

        AsyncGenerator(AsyncGenerator&& other) noexcept
            : IGenerator(), _coroutine(std::move(other._coroutine)), _destroyed(other._destroyed) {
            INTERNAL_COROUTINE_DEBUG("AsyncGenerator<{}>(AsyncGenerator&& other) {}", typeName<T>(), _coroutine.promise().get_id());
            other._coroutine = nullptr;
            if(_coroutine != nullptr) {
                // Assume we're moving because an iterator has not finished and has suspended
                _coroutine.promise()._hasSuspended = true;
            }
        }

        ~AsyncGenerator() final {
//            INTERNAL_COROUTINE_DEBUG("~AsyncGenerator<{}>() {} {} {}", typeName<T>(), *_destroyed, _coroutine == nullptr, !(*_destroyed) && _coroutine != nullptr ? _coroutine.promise().get_id() : 0);
            if (!(*_destroyed) && _coroutine) {
                if (_coroutine.promise().request_cancellation()) {
                    _coroutine.destroy();
                    *_destroyed = true;
                }
            }
        }

        AsyncGenerator& operator=(AsyncGenerator&& other) noexcept {
            INTERNAL_COROUTINE_DEBUG("operator=(AsyncGenerator<{}>&& other) {}", typeName<T>(), other._coroutine.promise().get_id());
            AsyncGenerator temp(std::move(other));
            swap(temp);
            if(_coroutine != nullptr) {
                // Assume we're moving because an iterator has not finished and has suspended
                _coroutine.promise()._hasSuspended = true;
            }
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
            return (*_destroyed) || _coroutine == nullptr || _coroutine.done();
        }

        void swap(AsyncGenerator& other) noexcept {
            using std::swap;
            swap(_coroutine, other._coroutine);
            swap(_destroyed, other._destroyed);
        }

        template <typename U = T> requires (!std::is_same_v<U, void>)
        [[nodiscard]] U& get_value() noexcept {
            return _coroutine.promise().value();
        }

    private:
        std::coroutine_handle<promise_type> _coroutine;
        std::shared_ptr<bool> _destroyed;
    };

    template<typename T>
    void swap(AsyncGenerator<T>& a, AsyncGenerator<T>& b) noexcept {
        a.swap(b);
    }
}
