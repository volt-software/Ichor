#pragma once

#include <utility>

namespace Ichor {
    template <typename T>
    struct CopyIsMoveWorkaround {
        explicit CopyIsMoveWorkaround(T&& obj) noexcept(noexcept(T(std::declval<T>()))) : _obj(std::forward<T>(obj)), _moved() {}

        CopyIsMoveWorkaround(const CopyIsMoveWorkaround& o) noexcept(noexcept(T(std::declval<T>()))) : _obj(std::move(const_cast<CopyIsMoveWorkaround<T>&>(o)._obj)), _moved() {
            const_cast<CopyIsMoveWorkaround<T>&>(o)._moved = true;
        }
        CopyIsMoveWorkaround& operator=(const CopyIsMoveWorkaround& o) noexcept(noexcept(std::is_nothrow_move_assignable_v<T>)) {
            if(o._moved) {
                throw std::runtime_error("Already moved");
            }

            _obj = std::move(const_cast<CopyIsMoveWorkaround<T>&>(o)._obj);
            o._moved = true;
            return *this;
        }

        CopyIsMoveWorkaround(CopyIsMoveWorkaround&& o) noexcept(noexcept(T(std::declval<T>()))) : _obj(std::move(o._obj)), _moved() {
            const_cast<CopyIsMoveWorkaround<T>&>(o)._moved = true;
        }
        CopyIsMoveWorkaround& operator=(CopyIsMoveWorkaround&& o) noexcept(noexcept(std::is_nothrow_move_assignable_v<T>)) {
            if(o._moved) {
                throw std::runtime_error("Already moved");
            }

            _obj = std::move(o._obj);
            o._moved = true;
            return *this;
        }

        T moveObject() {
            if(_moved) {
                throw std::runtime_error("Already moved");
            }

            _moved = true;
            return std::move(_obj);
        }

    private:
        T _obj;
        bool _moved;
    };
}