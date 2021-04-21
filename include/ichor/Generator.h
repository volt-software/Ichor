#pragma once

#include <cppcoro/generator.hpp>
#include <ichor/GetThreadLocalMemoryResource.h>

namespace Ichor{

    template<typename T>
    class Generator;

    namespace Detail {
        template<typename T>
        class GeneratorPromise {
        public:

            using value_type = std::remove_reference_t<T>;
            using reference_type = std::conditional_t<std::is_reference_v<T>, const T, const T &>;
            using pointer_type = value_type *;

            GeneratorPromise() = default;

            Generator<T> get_return_object() noexcept {
                using coroutine_handle = cppcoro::coroutine_handle<GeneratorPromise<T>>;
                return Generator<T>{coroutine_handle::from_promise(*this)};
            }

            constexpr cppcoro::suspend_always initial_suspend() const noexcept { return {}; }

            constexpr cppcoro::suspend_always final_suspend() const noexcept { return {}; }

            template<
                    typename U = T,
                    std::enable_if_t<!std::is_rvalue_reference<U>::value, int> = 0>
            cppcoro::suspend_always yield_value(std::remove_reference_t<T> &value) noexcept {
                ::new(static_cast<void *>(std::addressof(m_value))) T(std::forward<T>(value));
                return {};
            }

            cppcoro::suspend_always yield_value(std::remove_reference_t<T> &&value) noexcept {
                ::new(static_cast<void *>(std::addressof(m_value))) T(std::forward<T>(value));
                return {};
            }

            void unhandled_exception() noexcept {
                m_exception = std::current_exception();
            }

//            void return_void() noexcept {
//            }

            cppcoro::suspend_never return_value(std::remove_reference_t<T> &value) noexcept {
                ::new(static_cast<void *>(std::addressof(m_value))) T(std::forward<T>(value));
                return {};
            }

            cppcoro::suspend_never return_value(std::remove_reference_t<T> &&value) noexcept(std::is_nothrow_constructible_v<T, T &&>) {
                ::new(static_cast<void *>(std::addressof(m_value))) T(std::forward<T>(value));
                return {};
            }

            reference_type value() const noexcept {
                return m_value;
            }

            // Don't allow any use of 'co_await' inside the generator coroutine.
            template<typename U>
            cppcoro::suspend_never await_transform(U &&value) = delete;

            void rethrow_if_exception() {
                if (m_exception) {
                    std::rethrow_exception(m_exception);
                }
            }

            void *operator new(std::size_t sz) {
                auto* rsrc = getThreadLocalMemoryResource();
                auto* ptr = rsrc->allocate(sz);
                return ptr;
            }

            void operator delete(void *ptr, std::size_t sz) noexcept {
                auto rsrc = getThreadLocalMemoryResource();
                rsrc->deallocate(ptr, sz);
            }

        private:

            T m_value;
            std::exception_ptr m_exception;

        };

        struct GeneratorSentinel {
        };

        template<typename T>
        class GeneratorIterator {
            using coroutine_handle = cppcoro::coroutine_handle<GeneratorPromise<T>>;

        public:

            using iterator_category = std::input_iterator_tag;
            // What type should we use for counting elements of a potentially infinite sequence?
            using difference_type = std::ptrdiff_t;
            using value_type = typename GeneratorPromise<T>::value_type;
            using reference = typename GeneratorPromise<T>::reference_type;
            using pointer = typename GeneratorPromise<T>::pointer_type;

            // Iterator needs to be default-constructible to satisfy the Range concept.
            GeneratorIterator() noexcept
                    : m_coroutine(nullptr) {}

            explicit GeneratorIterator(coroutine_handle coroutine) noexcept
                    : m_coroutine(coroutine) {}

            friend bool operator==(const GeneratorIterator &it, GeneratorSentinel) noexcept {
                return !it.m_coroutine || it.m_coroutine.done();
            }

            friend bool operator!=(const GeneratorIterator &it, GeneratorSentinel s) noexcept {
                return !(it == s);
            }

            friend bool operator==(GeneratorSentinel s, const GeneratorIterator &it) noexcept {
                return (it == s);
            }

            friend bool operator!=(GeneratorSentinel s, const GeneratorIterator &it) noexcept {
                return it != s;
            }

            GeneratorIterator &operator++() {
                m_coroutine.resume();
                if (m_coroutine.done()) {
                    m_coroutine.promise().rethrow_if_exception();
                }

                return *this;
            }

            // Need to provide post-increment operator to implement the 'Range' concept.
            void operator++(int) {
                (void) operator++();
            }

            reference operator*() const noexcept {
                return m_coroutine.promise().value();
            }

            pointer operator->() const noexcept {
                return std::addressof(operator*());
            }

        private:

            coroutine_handle m_coroutine;
        };
    }

    template<typename T>
    class [[nodiscard]] Generator
    {
    public:

        using promise_type = Detail::GeneratorPromise<T>;
        using iterator = Detail::GeneratorIterator<T>;

        Generator() noexcept
                : m_coroutine(nullptr)
        {}

        Generator(Generator&& other) noexcept
                : m_coroutine(other.m_coroutine)
        {
            other.m_coroutine = nullptr;
        }

        Generator(const Generator& other) = delete;

        ~Generator()
        {
            if (m_coroutine)
            {
                m_coroutine.destroy();
            }
        }

        Generator& operator=(Generator other) noexcept
        {
            swap(other);
            return *this;
        }

        iterator begin()
        {
            if (m_coroutine)
            {
                m_coroutine.resume();
                if (m_coroutine.done())
                {
                    m_coroutine.promise().rethrow_if_exception();
                }
            }

            return iterator{ m_coroutine };
        }

        Detail::GeneratorSentinel end() noexcept
        {
            return Detail::GeneratorSentinel{};
        }

        void swap(Generator& other) noexcept
        {
            std::swap(m_coroutine, other.m_coroutine);
        }

    private:

        friend class Detail::GeneratorPromise<T>;

        explicit Generator(cppcoro::coroutine_handle<promise_type> coroutine) noexcept
                : m_coroutine(coroutine)
        {}

        cppcoro::coroutine_handle<promise_type> m_coroutine;

    };
}