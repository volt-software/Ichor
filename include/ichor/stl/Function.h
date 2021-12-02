#pragma once

#include <memory_resource>
#include <ichor/stl/Common.h>

// Differs from std::function by explicitly requiring a memory_resource to allocate from
// Maybe doesn't work in some situations where std::function would, missing tests
namespace Ichor {

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
        function(T&& t, std::pmr::memory_resource *rsrc) : _callable(new (rsrc->allocate(sizeof(callable<std::decay_t<T>>))) callable<std::decay_t<T>> (std::forward<T>(t)), Deleter{InternalDeleter<callable<std::decay_t<T>>>{rsrc}}) { }

        function(const function&) = delete;
        function(function&& other) noexcept = default;
        function& operator=(const function&) = delete;
        function& operator=(function&&) noexcept = default;

        ReturnValue operator()(Args... args) const {
            return _callable->invoke(args...);
        }
    };
}