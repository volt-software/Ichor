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
        explicit ScopeGuardFunction(T &&fn) noexcept : _fn(std::forward<T>(fn)) {
        }

        ~ScopeGuardFunction() noexcept {
            _fn();
        }
    private:
        T _fn;
    };
}
