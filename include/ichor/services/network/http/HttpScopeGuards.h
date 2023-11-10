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

        ScopeGuardAtomicCount() = delete;
        ScopeGuardAtomicCount(const ScopeGuardAtomicCount &) = delete;
        ScopeGuardAtomicCount(ScopeGuardAtomicCount &&) = delete;
        ScopeGuardAtomicCount& operator=(const ScopeGuardAtomicCount&) = delete;
        ScopeGuardAtomicCount& operator=(ScopeGuardAtomicCount&&) = delete;
    private:
        std::atomic<int64_t> &_var;
    };
}
