#pragma once

#include <memory_resource>

// Differs from std::function by explicitly requiring a memory_resource to allocate from
// Maybe doesn't work in some situations where std::function would, missing tests
namespace Ichor {

    struct Deleter final {
        Deleter(std::pmr::memory_resource *rsrc, std::size_t size) noexcept : _rsrc(rsrc), _size(size) {}
        Deleter() noexcept = default;

        Deleter(const Deleter&) noexcept = default;
        Deleter(Deleter&&) noexcept = default;

        Deleter& operator=(const Deleter&) noexcept = default;
        Deleter& operator=(Deleter&&) noexcept = default;

        ~Deleter() noexcept {
            if(_rsrc == nullptr) {
                std::terminate();
            }
        }

        //Called by unique_ptr or shared_ptr to destroy/free the Resource
        void operator()(void *c) const noexcept {
            if(c != nullptr) {
                _rsrc->deallocate(c, _size);
            }
        }

        std::pmr::memory_resource *_rsrc{nullptr};
        std::size_t _size{};
    };

    template <typename>
    struct function; // no definition

    template <typename ReturnValue, typename... Args>
    struct function<ReturnValue(Args...)>
    {
    private:
        struct callable_base
        {
            callable_base() = default;
            virtual ~callable_base() = default;
            virtual ReturnValue invoke(Args...) const = 0;
        };

        template<typename T>
        struct callable final : callable_base
        {
            T _t;

            explicit callable(T const& t) : _t(t) {}
            explicit callable(T&& t) noexcept : _t(std::move(t)) {}
            callable& operator=(const callable& c) {
                _t = c._t;
            }
            callable& operator=(callable&&) noexcept = default;

            ReturnValue invoke(Args... args) const final
            {
                return _t(args...);
            }
        };

        std::unique_ptr<callable_base, Deleter> _callable;

    public:

        template<typename T>
        function(T&& t, std::pmr::memory_resource *rsrc) : _callable(new (rsrc->allocate(sizeof(callable<std::decay_t<T>>))) callable<std::decay_t<T>> (std::forward<T>(t)), Deleter{rsrc, sizeof(callable<std::decay_t<T>>)}) { }

        function(const function&) = delete;
        function(function&& other) noexcept = default;
        function& operator=(const function&) = delete;
        function& operator=(function&&) noexcept = default;

        ReturnValue operator()(Args... args) const {
            return _callable->invoke(args...);
        }
    };
}