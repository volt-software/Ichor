#pragma once

#include <atomic>
#include <tl/function_ref.h>

namespace Ichor {
    class ScopeGuardAtomicCount final {
    public:
        explicit ScopeGuardAtomicCount(std::atomic<int64_t> &var) noexcept : _var(var) {
            _var.fetch_add(1, std::memory_order_acq_rel);
        }

        ~ScopeGuardAtomicCount() noexcept {
            _var.fetch_sub(1, std::memory_order_acq_rel);
        }
    private:
        std::atomic<int64_t> &_var;
    };

    template <typename T>
    class ScopeGuardFunction final {
    public:
        static_assert(std::is_trivially_copyable_v<T>, "ScopeGuardFunction only works with trivially copyable types.");
        static_assert(std::is_trivially_copy_constructible_v<T>, "ScopeGuardFunction only works with trivially copy constructible types.");
        explicit ScopeGuardFunction(T t, tl::function_ref<void(T&)> fn) noexcept : _t(t), _fn(fn) {
        }

        ~ScopeGuardFunction() noexcept {
            _fn(_t);
        }
    private:
        T _t;
        tl::function_ref<void(T&)> _fn;
    };
}