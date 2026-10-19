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
#include <ichor/stl/CompilerSpecific.h>

#ifdef ICHOR_HAVE_STD_STACKTRACE
#include <stacktrace>
#endif

namespace Ichor {
    template<typename T>
    class ICHOR_CORO_AWAIT_ELIDABLE ICHOR_CORO_LIFETIME_BOUND AsyncGenerator;

    namespace v1::Detail {
        template <typename T>
        class AsyncGeneratorAdvanceOperation {
        protected:
            explicit constexpr AsyncGeneratorAdvanceOperation(std::nullptr_t) noexcept
                : _promise(nullptr)
                , _producerCoroutine(nullptr)
            {
            }

            constexpr AsyncGeneratorAdvanceOperation(
                AsyncGeneratorPromiseBase& promise,
                std::coroutine_handle<AsyncGeneratorPromise<T>> producerCoroutine) noexcept
                : _promise(std::addressof(promise))
                , _producerCoroutine(producerCoroutine)
            {
                state initialState = promise._state;
                if (initialState == state::value_ready_producer_suspended) {
                    promise._state = state::value_not_ready_consumer_active;

                    producerCoroutine.resume();

                    initialState = promise._state;

                    // Neither yielded nor finished: the producer suspended on something else (e.g. an event or a Task)
                    // and will be resumed asynchronously. When it then finishes, it needs to notify the DependencyManager.
                    if (initialState == state::value_not_ready_consumer_active && !promise.finished()) {
                        promise._hasSuspended = true;
                    }
                }

                _initialState = initialState;
            }

        public:
            [[nodiscard]] ICHOR_COROUTINE_CONSTEXPR bool await_ready() const noexcept {
                INTERNAL_COROUTINE_DEBUG("AsyncGeneratorAdvanceOperation::await_ready {} {}", _initialState, _promise->_id);
                return _initialState == state::value_ready_producer_suspended;
            }

            ICHOR_COROUTINE_CONSTEXPR bool await_suspend(std::coroutine_handle<> consumerCoroutine) noexcept {
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

#ifdef ICHOR_HAVE_STD_STACKTRACE
                        _promise->set_stacktrace(std::stacktrace::current());
#endif

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

#ifdef ICHOR_HAVE_STD_STACKTRACE
                    _promise->set_stacktrace(std::stacktrace::current());
#endif
                    return true;
                }
                INTERNAL_COROUTINE_DEBUG("AsyncGeneratorAdvanceOperation::await_suspend4");
                return false;
            }

        protected:
            AsyncGeneratorPromiseBase* _promise;
            std::coroutine_handle<AsyncGeneratorPromise<T>> _producerCoroutine;

        public:
            state _initialState{};
        };

        template<typename T>
        class AsyncGeneratorIncrementOperation final : public AsyncGeneratorAdvanceOperation<T> {
        public:
            explicit constexpr AsyncGeneratorIncrementOperation(AsyncGeneratorIterator<T>& iterator) noexcept
                : AsyncGeneratorAdvanceOperation<T>(iterator._coroutine.promise(), iterator._coroutine)
                , _iterator(iterator)
            {
            }

            ICHOR_COROUTINE_CONSTEXPR AsyncGeneratorIterator<T>& await_resume();

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

            explicit constexpr AsyncGeneratorIterator(std::nullptr_t) noexcept
                : _coroutine(nullptr)
            {}

            explicit constexpr AsyncGeneratorIterator(handle_type coroutine) noexcept
                : _coroutine(coroutine)
            {}

            constexpr AsyncGeneratorIncrementOperation<T> operator++() noexcept {
                return AsyncGeneratorIncrementOperation<T>{ *this };
            }

            constexpr reference operator*() const noexcept {
                return _coroutine.promise().value();
            }

            constexpr bool operator==(const AsyncGeneratorIterator& other) const noexcept {
                return _coroutine == other._coroutine;
            }

            constexpr bool operator!=(const AsyncGeneratorIterator& other) const noexcept {
                return !(*this == other);
            }

        private:
            friend class AsyncGeneratorIncrementOperation<T>;
            handle_type _coroutine;
        };

        template<typename T>
        ICHOR_COROUTINE_CONSTEXPR AsyncGeneratorIterator<T>& AsyncGeneratorIncrementOperation<T>::await_resume() {
            INTERNAL_COROUTINE_DEBUG("AsyncGeneratorIncrementOperation<{}>::await_resume {}", typeName<T>(), this->_promise->_id);
            if (this->_promise->finished()) {
                // Update iterator to end()
                _iterator = AsyncGeneratorIterator<T>{ nullptr };
            }

            return _iterator;
        }

        template<typename T>
        class AsyncGeneratorBeginOperation final : public AsyncGeneratorAdvanceOperation<T>, public IAsyncGeneratorBeginOperation
        {
            using promise_type = AsyncGeneratorPromise<T>;
            using handle_type = std::coroutine_handle<promise_type>;

        public:

            explicit ICHOR_COROUTINE_CONSTEXPR AsyncGeneratorBeginOperation(std::nullptr_t) noexcept
                : AsyncGeneratorAdvanceOperation<T>(nullptr) {
                INTERNAL_COROUTINE_DEBUG("AsyncGeneratorBeginOperation<{}>(nullptr) {}", typeName<T>(), this->_initialState);
            }

            explicit ICHOR_COROUTINE_CONSTEXPR AsyncGeneratorBeginOperation(handle_type producerCoroutine) noexcept
                : AsyncGeneratorAdvanceOperation<T>(producerCoroutine.promise(), producerCoroutine) {
                INTERNAL_COROUTINE_DEBUG("AsyncGeneratorBeginOperation<{}>(producerCoroutine) {} {}", typeName<T>(), this->_initialState, this->_promise->_state);
            }

            constexpr ~AsyncGeneratorBeginOperation() final = default;

            [[nodiscard]] ICHOR_COROUTINE_CONSTEXPR bool await_ready() const noexcept {
                INTERNAL_COROUTINE_DEBUG("AsyncGeneratorBeginOperation<{}>::await_ready {}", typeName<T>(), this->_promise == nullptr ? 0u : this->_promise->_id);
                return this->_promise == nullptr || AsyncGeneratorAdvanceOperation<T>::await_ready();
            }

            [[nodiscard]] ICHOR_COROUTINE_CONSTEXPR bool get_finished() const noexcept final {
                INTERNAL_COROUTINE_DEBUG("AsyncGeneratorBeginOperation<{}>::get_finished {} {}", typeName<T>(), this->_promise == nullptr, this->_promise == nullptr ? false : this->_promise->finished());
                return this->_promise == nullptr || this->_promise->finished();
            }

            /// Whenever a consumer/producer routine suspends, the generator promise sets the value.
            /// If the coroutine never ends up in await_suspend, f.e. because of co_await'ing on another coroutine immediately,
            /// suspended is in an un-set state and is considered as suspended.
            /// \return true if suspended or never engaged
            [[nodiscard]] constexpr bool get_has_suspended() const noexcept final {
                return this->_promise != nullptr && (!this->_promise->_hasSuspended.has_value() || *this->_promise->_hasSuspended);
            }

            [[nodiscard]] constexpr state get_op_state() const noexcept final {
                return this->_initialState;
            }

            [[nodiscard]] constexpr state get_promise_state() const noexcept final {
                if(this->_promise == nullptr) {
                    return state::unknown;
                }
                return this->_promise->_state;
            }

            [[nodiscard]] constexpr bool promise_is_null() const noexcept {
                return this->_promise == nullptr;
            }

            [[nodiscard]] constexpr uint64_t get_promise_id() const noexcept final {
                if(this->_promise == nullptr) {
                    return 0;
                }

                return this->_promise->get_id();
            }

            constexpr void set_priority(uint64_t priority) noexcept {
                this->_promise->set_priority(priority);
            }

            template <typename U = T> requires (!std::is_same_v<U, void>)
            [[nodiscard]] constexpr U& get_value() noexcept {
                auto prom = std::coroutine_handle<promise_type>::from_promise(*static_cast<promise_type*>(this->_promise));
                return prom.promise().value();
            }

            ICHOR_COROUTINE_CONSTEXPR AsyncGeneratorIterator<T> await_resume() {
                if (this->_promise == nullptr) {
                    INTERNAL_COROUTINE_DEBUG("AsyncGeneratorIterator<{}>::await_resume NULL", typeName<T>());
                    // Called begin() on the empty generator.
                    return AsyncGeneratorIterator<T>{ nullptr };
                }
                INTERNAL_COROUTINE_DEBUG("AsyncGeneratorIterator<{}>::await_resume {}", typeName<T>(), this->_promise->_id);

                return AsyncGeneratorIterator<T>{
                    handle_type::from_promise(*static_cast<promise_type*>(this->_promise))
                };
            }
        };

        inline ICHOR_COROUTINE_CONSTEXPR bool AsyncGeneratorYieldOperation::await_suspend(std::coroutine_handle<> producer) noexcept {
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

#ifdef ICHOR_HAVE_STD_STACKTRACE
                    _promise.set_stacktrace(std::stacktrace::current());
#endif

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

#ifdef ICHOR_HAVE_STD_STACKTRACE
                    _promise.set_stacktrace(std::stacktrace::current());
#endif
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

        inline ICHOR_COROUTINE_CONSTEXPR AsyncGeneratorYieldOperation AsyncGeneratorPromiseBase::final_suspend() noexcept {
            INTERNAL_COROUTINE_DEBUG("AsyncGeneratorPromiseBase::final_suspend {}", _id);
            set_finished();
            return internal_yield_value();
        }

        inline constexpr AsyncGeneratorYieldOperation AsyncGeneratorPromiseBase::internal_yield_value() noexcept {
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
    class ICHOR_CORO_AWAIT_ELIDABLE ICHOR_CORO_LIFETIME_BOUND AsyncGenerator final : public IGenerator {
    public:
        using promise_type = v1::Detail::AsyncGeneratorPromise<T>;
        using iterator = v1::Detail::AsyncGeneratorIterator<T>;

        explicit ICHOR_COROUTINE_CONSTEXPR AsyncGenerator(promise_type& promise) noexcept
            : IGenerator(), _coroutine(std::coroutine_handle<promise_type>::from_promise(promise)) {
            INTERNAL_COROUTINE_DEBUG("AsyncGenerator<{}>(promise_type& promise) {}", typeName<T>(), debug_id());
            promise.set_owner(this);
        }

        ICHOR_COROUTINE_CONSTEXPR AsyncGenerator(AsyncGenerator&& other) noexcept
            : IGenerator(), _coroutine(std::exchange(other._coroutine, nullptr)) {
            INTERNAL_COROUTINE_DEBUG("AsyncGenerator<{}>(AsyncGenerator&& other) {}", typeName<T>(), debug_id());
            if(_coroutine) {
                _coroutine.promise().set_owner(this);
            }
        }

        ICHOR_COROUTINE_CONSTEXPR ~AsyncGenerator() final {
            INTERNAL_COROUTINE_DEBUG("~AsyncGenerator<{}>() {}", typeName<T>(), debug_id());
            release();
        }

        ICHOR_COROUTINE_CONSTEXPR AsyncGenerator& operator=(AsyncGenerator&& other) noexcept {
            INTERNAL_COROUTINE_DEBUG("operator=(AsyncGenerator<{}>&& other) {} {}", typeName<T>(), debug_id(), other.debug_id());
            if(this != std::addressof(other)) {
                release();
                _coroutine = std::exchange(other._coroutine, nullptr);
                if(_coroutine) {
                    _coroutine.promise().set_owner(this);
                }
            }
            return *this;
        }

        AsyncGenerator(const AsyncGenerator&) = delete;
        AsyncGenerator& operator=(const AsyncGenerator&) = delete;

        [[nodiscard]] ICHOR_COROUTINE_CONSTEXPR auto begin() noexcept {
            INTERNAL_COROUTINE_DEBUG("AsyncGenerator<{}>::begin() {}", typeName<T>(), debug_id());
            if (!_coroutine) {
                return v1::Detail::AsyncGeneratorBeginOperation<T>{ nullptr };
            }

            return v1::Detail::AsyncGeneratorBeginOperation<T>{ _coroutine };
        }

        [[nodiscard]] AsyncGeneratorBeginOperationHolder begin_interface() noexcept final {
            INTERNAL_COROUTINE_DEBUG("AsyncGenerator<{}>::begin_interface() {}", typeName<T>(), debug_id());
            if (!_coroutine) {
                return AsyncGeneratorBeginOperationHolder{ std::in_place_type<v1::Detail::AsyncGeneratorBeginOperation<T>>, nullptr };
            }

            return AsyncGeneratorBeginOperationHolder{ std::in_place_type<v1::Detail::AsyncGeneratorBeginOperation<T>>, _coroutine };
        }

        constexpr auto end() noexcept {
            return iterator{ nullptr };
        }

        [[nodiscard]] ICHOR_COROUTINE_CONSTEXPR bool done() const noexcept final {
            INTERNAL_COROUTINE_DEBUG("AsyncGenerator<{}>::done() {} {}", typeName<T>(), debug_id(), debug_state());
            return _coroutine == nullptr || _coroutine.done();
        }

        ICHOR_COROUTINE_CONSTEXPR void swap(AsyncGenerator& other) noexcept {
            INTERNAL_COROUTINE_DEBUG("AsyncGenerator<{}>::swap() {} {}", typeName<T>(), debug_id(), other.debug_id());
            using std::swap;
            swap(_coroutine, other._coroutine);
            if(_coroutine) {
                _coroutine.promise().set_owner(this);
            }
            if(other._coroutine) {
                other._coroutine.promise().set_owner(std::addressof(other));
            }
        }

        template <typename U = T> requires (!std::is_same_v<U, void>)
        [[nodiscard]] ICHOR_COROUTINE_CONSTEXPR U& get_value() noexcept {
            INTERNAL_COROUTINE_DEBUG("AsyncGenerator<{}>::get_value() {} {} {}", typeName<T>(), typeName<U>(), debug_id(), debug_state());
            return promise().value();
        }

        ICHOR_COROUTINE_CONSTEXPR void set_priority(uint64_t priority) noexcept {
            INTERNAL_COROUTINE_DEBUG("AsyncGenerator<{}>::set_priority({}) {} {}", typeName<T>(), priority, debug_id(), debug_state());
            promise().set_priority(priority);
        }

        [[nodiscard]] constexpr const ServiceIdType get_service_id() const noexcept final {
            INTERNAL_COROUTINE_DEBUG("AsyncGenerator<{}>::get_service_id() {} {}", typeName<T>(), debug_id(), debug_state());
            return promise().get_service_id();
        }

        constexpr void set_service_id(ServiceIdType svcId) noexcept final {
            INTERNAL_COROUTINE_DEBUG("AsyncGenerator<{}>::set_service_id() {} {}", typeName<T>(), debug_id(), debug_state());
            return promise().set_service_id(svcId);
        }

#ifdef ICHOR_HAVE_STD_STACKTRACE
        [[nodiscard]] constexpr std::stacktrace const &get_stacktrace() const noexcept final {
            return promise().get_stacktrace();
        }
#endif

    private:
        friend class v1::Detail::AsyncGeneratorPromise<T>;

        /// Called by the promise when the coroutine gets destroyed.
        constexpr void on_coroutine_destroyed() noexcept {
            _coroutine = nullptr;
        }

        /// Cancels the coroutine. If it is suspended at a co_yield/co_return it is destroyed right away,
        /// otherwise it is detached and destroys itself when it reaches the next co_yield/co_return.
        ICHOR_COROUTINE_CONSTEXPR void release() noexcept {
            if (!_coroutine) {
                return;
            }

            auto coroutine = std::exchange(_coroutine, nullptr);
            coroutine.promise().set_owner(nullptr);
            if (coroutine.promise().request_cancellation()) {
                coroutine.destroy();
            }
        }

        [[nodiscard]] constexpr promise_type& promise() const noexcept {
            assert(_coroutine);
#ifdef ICHOR_USE_HARDENING
            if(!_coroutine) [[unlikely]] {
                std::terminate();
            }
#endif
            return _coroutine.promise();
        }

        [[nodiscard]] constexpr uint64_t debug_id() const noexcept {
            return _coroutine ? _coroutine.promise().get_id() : 0;
        }

        [[nodiscard]] constexpr state debug_state() const noexcept {
            return _coroutine ? _coroutine.promise()._state : state::unknown;
        }

        // Reset to nullptr by the promise if the coroutine gets destroyed before this AsyncGenerator.
        std::coroutine_handle<promise_type> _coroutine;
    };

    template<typename T>
    constexpr void swap(AsyncGenerator<T>& a, AsyncGenerator<T>& b) noexcept {
        a.swap(b);
    }

    namespace v1::Detail {
        template<typename T>
        inline AsyncGeneratorPromise<T>::~AsyncGeneratorPromise() {
            INTERNAL_COROUTINE_DEBUG("~AsyncGeneratorPromise<{}>()", typeName<T>());
            if(_owner != nullptr) {
                _owner->on_coroutine_destroyed();
            }
        }

        inline AsyncGeneratorPromise<void>::~AsyncGeneratorPromise() {
            INTERNAL_COROUTINE_DEBUG("~AsyncGeneratorPromise<>()");
            if(_owner != nullptr) {
                _owner->on_coroutine_destroyed();
            }
        }
    }
}
