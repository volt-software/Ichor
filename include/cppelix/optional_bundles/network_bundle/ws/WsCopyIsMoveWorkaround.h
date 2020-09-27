#pragma once

#include <utility>

namespace Cppelix {
    template <typename T>
    struct CopyIsMoveWorkaround {
        explicit CopyIsMoveWorkaround(T&& obj) noexcept(noexcept(T(std::declval<T>()))) : _obj(std::forward<T>(obj)) {}

        CopyIsMoveWorkaround(const CopyIsMoveWorkaround& o) noexcept(noexcept(T(std::declval<T>()))) : _obj(std::move(const_cast<CopyIsMoveWorkaround<T>&>(o)._obj)) {}
        CopyIsMoveWorkaround& operator=(const CopyIsMoveWorkaround& o) noexcept(noexcept(std::is_nothrow_move_assignable_v<T, T>)) {
            _obj = std::move(const_cast<CopyIsMoveWorkaround<T>&>(o)._obj);
            return *this;
        }

        CopyIsMoveWorkaround(CopyIsMoveWorkaround&& o) noexcept(noexcept(T(std::declval<T>()))) : _obj(std::move(o._obj)) {}
        CopyIsMoveWorkaround& operator=(CopyIsMoveWorkaround&& o) noexcept(noexcept(std::is_nothrow_move_assignable_v<T, T>)) {
            _obj = std::move(o._obj);
            return *this;
        }
        T _obj;
    };
}