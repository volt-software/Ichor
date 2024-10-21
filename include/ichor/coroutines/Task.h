///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

// Copied and modified from Lewis Baker's cppcoro

#pragma once

#include <exception>
#include <cstdint>
#include <cassert>
#include <coroutine>
#include <new>

namespace Ichor
{
    template<typename T> class Task;

    namespace Detail
    {
        class [[nodiscard]] TaskPromiseBase
        {
            friend struct final_awaitable;

            struct final_awaitable
            {
                constexpr bool await_ready() const noexcept { return false; }

                template<typename PROMISE>
                constexpr void await_suspend(std::coroutine_handle<PROMISE> coroutine) noexcept {
                    coroutine.promise().m_continuation.resume();
                }

                constexpr void await_resume() noexcept {}
            };

        public:

            constexpr TaskPromiseBase() noexcept
            {}

            constexpr auto initial_suspend() noexcept {
                return std::suspend_always{};
            }

            constexpr auto final_suspend() noexcept {
                return final_awaitable{};
            }

            constexpr uint64_t get_id() const noexcept {
                return _promise_id;
            };

            template <typename PROMISE>
            constexpr void set_continuation(std::coroutine_handle<PROMISE> continuation) noexcept {
                _promise_id = continuation.promise().get_id();
                m_continuation = continuation;
            }

            [[nodiscard]] constexpr bool done() const noexcept {
                return m_continuation == nullptr || m_continuation.done();
            }

        private:
            std::coroutine_handle<> m_continuation;
            uint64_t _promise_id{};
        };

        template<typename T>
        class [[nodiscard]] TaskPromise final : public TaskPromiseBase
        {
        public:

            constexpr TaskPromise() noexcept {}

            ~TaskPromise() {
                switch (m_resultType) {
                    case result_type::value:
                        m_value.~T();
                        break;
                    case result_type::exception:
                        m_exception.~exception_ptr();
                        break;
                    default:
                        break;
                }
            }

            [[nodiscard]]
#if __cpp_constexpr >= 202207L
            constexpr
#endif
            Task<T> get_return_object() noexcept;

            constexpr void unhandled_exception() noexcept {
                ::new (static_cast<void*>(std::addressof(m_exception))) std::exception_ptr(
                        std::current_exception());
                m_resultType = result_type::exception;
            }

            template<
                    typename VALUE,
                    typename = std::enable_if_t<std::is_convertible_v<VALUE&&, T>>>
            constexpr void return_value(VALUE&& value)
            noexcept(std::is_nothrow_constructible_v<T, VALUE&&>) {
                ::new (static_cast<void*>(std::addressof(m_value))) T(std::forward<VALUE>(value));
                m_resultType = result_type::value;
            }

            constexpr void return_value(T&& value) noexcept {
                ::new (static_cast<void*>(std::addressof(m_value))) T(std::forward<T>(value));
                m_resultType = result_type::value;
            }

            [[nodiscard]]
#if __cpp_constexpr >= 202207L
            constexpr
#endif
            T& result() & {
                if (m_resultType == result_type::exception) {
                    std::rethrow_exception(m_exception);
                }

                assert(m_resultType == result_type::value);

                return m_value;
            }

            // HACK: Need to have co_await of Task<int> return prvalue rather than
            // rvalue-reference to work around an issue with MSVC where returning
            // rvalue reference of a fundamental type from await_resume() will
            // cause the value to be copied to a temporary. This breaks the
            // sync_wait() implementation.
            // See https://github.com/lewissbaker/cppcoro/issues/40#issuecomment-326864107
            using rvalue_type = std::conditional_t<
                    std::is_arithmetic_v<T> || std::is_pointer_v<T>,
                    T,
                    T&&>;

            [[nodiscard]]
#if __cpp_constexpr >= 202207L
            constexpr
#endif
            rvalue_type result() && {
                if (m_resultType == result_type::exception) {
                    std::rethrow_exception(m_exception);
                }

                assert(m_resultType == result_type::value);

                return std::move(m_value);
            }

        private:

            enum class result_type : uint_fast16_t { empty, value, exception };

            result_type m_resultType = result_type::empty;

            union {
                T m_value;
                std::exception_ptr m_exception;
            };

        };

        template<>
        class [[nodiscard]] TaskPromise<void> : public TaskPromiseBase {
        public:

            TaskPromise() noexcept = default;

            [[nodiscard]]
#if __cpp_constexpr >= 202207L
            constexpr
#endif
            Task<void> get_return_object() noexcept;

            constexpr void return_void() noexcept
            {}

            void unhandled_exception() noexcept {
                m_exception = std::current_exception();
            }

#if __cpp_constexpr >= 202207L
            constexpr
#endif
            void result() {
                if (m_exception) {
                    std::rethrow_exception(m_exception);
                }
            }

        private:

            std::exception_ptr m_exception;

        };

        template<typename T>
        class [[nodiscard]] TaskPromise<T&> : public TaskPromiseBase {
        public:

            TaskPromise() noexcept = default;

            [[nodiscard]]
#if __cpp_constexpr >= 202207L
            constexpr
#endif
            Task<T&> get_return_object() noexcept;

            constexpr void unhandled_exception() noexcept {
                m_exception = std::current_exception();
            }

            constexpr void return_value(T& value) noexcept {
                m_value = std::addressof(value);
            }

            [[nodiscard]]
#if __cpp_constexpr >= 202207L
            constexpr
#endif
            T& result() {
                if (m_exception) {
                    std::rethrow_exception(m_exception);
                }

                return *m_value;
            }

        private:

            T* m_value = nullptr;
            std::exception_ptr m_exception;

        };
    }

    /// \brief
    /// A Task represents an operation that produces a result both lazily
    /// and asynchronously.
    ///
    /// When you call a coroutine that returns a Task, the coroutine
    /// simply captures any passed parameters and returns exeuction to the
    /// caller. Execution of the coroutine body does not start until the
    /// coroutine is first co_await'ed.
    template<typename T = void>
    class [[nodiscard]] Task
    {
    public:

        using promise_type = Detail::TaskPromise<T>;

        using value_type = T;

    private:

        struct awaitable_base
        {
            std::coroutine_handle<promise_type> m_coroutine;

            constexpr awaitable_base(std::coroutine_handle<promise_type> coroutine) noexcept
                    : m_coroutine(coroutine)
            {}

            constexpr bool await_ready() const noexcept {
                return !m_coroutine || m_coroutine.done();
            }

            template <typename PROMISE>
            constexpr void await_suspend(std::coroutine_handle<PROMISE> awaitingCoroutine) noexcept {
                m_coroutine.promise().set_continuation(awaitingCoroutine);
                m_coroutine.resume();
            }
        };

    public:

        constexpr Task() noexcept
                : m_coroutine(nullptr)
        {}

        explicit constexpr Task(std::coroutine_handle<promise_type> coroutine)
                : m_coroutine(coroutine)
        {}

        constexpr Task(Task&& t) noexcept
                : m_coroutine(t.m_coroutine) {
            t.m_coroutine = nullptr;
        }

        /// Disable copy construction/assignment.
        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;

        /// Frees resources used by this Task.
        constexpr ~Task() {
            if (m_coroutine) {
                m_coroutine.destroy();
            }
        }

        constexpr Task& operator=(Task&& other) noexcept {
            if (std::addressof(other) != this) {
                if (m_coroutine) {
                    m_coroutine.destroy();
                }

                m_coroutine = other.m_coroutine;
                other.m_coroutine = nullptr;
            }

            return *this;
        }

        /// \brief
        /// Query if the Task result is complete.
        ///
        /// Awaiting a Task that is ready is guaranteed not to block/suspend.
        constexpr bool is_ready() const noexcept {
            return m_coroutine && m_coroutine.done();
        }

        [[nodiscard]] constexpr auto operator co_await() const & noexcept {
            struct awaitable : awaitable_base {
                using awaitable_base::awaitable_base;

                decltype(auto) await_resume() {
                    if (!this->m_coroutine) {
                        std::terminate();
                    }

                    return this->m_coroutine.promise().result();
                }
            };

            return awaitable{ m_coroutine };
        }

        [[nodiscard]] constexpr auto operator co_await() const && noexcept {
            struct awaitable : awaitable_base {
                using awaitable_base::awaitable_base;

                decltype(auto) await_resume() {
                    if (!this->m_coroutine) {
                        std::terminate();
                    }

                    return std::move(this->m_coroutine.promise()).result();
                }
            };

            return awaitable{ m_coroutine };
        }

        /// \brief
        /// Returns an awaitable that will await completion of the Task without
        /// attempting to retrieve the result.
        constexpr auto when_ready() const noexcept {
            struct awaitable : awaitable_base {
                using awaitable_base::awaitable_base;

                constexpr void await_resume() const noexcept {}
            };

            return awaitable{ m_coroutine };
        }

    private:
        std::coroutine_handle<promise_type> m_coroutine;
    };

    namespace Detail {
        template<typename T>
        [[nodiscard]]
#if __cpp_constexpr >= 202207L
        constexpr
#endif
        Task<T> TaskPromise<T>::get_return_object() noexcept {
            return Task<T>{ std::coroutine_handle<TaskPromise>::from_promise(*this) };
        }

        [[nodiscard]] inline
#if __cpp_constexpr >= 202207L
        constexpr
#endif
        Task<void> TaskPromise<void>::get_return_object() noexcept {
            return Task<void>{ std::coroutine_handle<TaskPromise>::from_promise(*this) };
        }

        template<typename T>
        [[nodiscard]]
#if __cpp_constexpr >= 202207L
        constexpr
#endif
        Task<T&> TaskPromise<T&>::get_return_object() noexcept {
            return Task<T&>{ std::coroutine_handle<TaskPromise>::from_promise(*this) };
        }
    }
}
