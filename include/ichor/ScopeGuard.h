#pragma once

#include <type_traits>
#include <utility>

namespace Ichor {

    template<typename Callback>
    class [[nodiscard]] ScopeGuard final
    {
    public:
        ScopeGuard(ScopeGuard&& o) noexcept(std::is_nothrow_constructible_v<Callback, Callback&&>) : _callback(std::forward<Callback>(o._callback)), _active(o._active)  {
            o._active = false;
        }
        explicit ScopeGuard(Callback&& callback) noexcept(std::is_nothrow_constructible_v<Callback, Callback&&>) : _callback(std::forward<Callback>(callback)), _active(true) {

        }

        ~ScopeGuard() noexcept {
            if(_active) {
                _callback();
            }
        }

        ScopeGuard() = delete;
        ScopeGuard(const ScopeGuard&) = delete;
        ScopeGuard& operator=(const ScopeGuard&) = delete;
        ScopeGuard& operator=(ScopeGuard&&) = delete;

    private:
        Callback _callback;
        bool _active;
    };
}
