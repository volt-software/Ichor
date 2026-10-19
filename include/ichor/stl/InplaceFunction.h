#pragma once

#include <cstddef>
#include <array>
#include <exception>
#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

// A move-only std::function alternative that never heap allocates: the callable is stored in a fixed-size inline buffer.
// Callables that do not fit result in a compile error.
namespace Ichor::v1 {
    template <typename Signature, std::size_t Capacity>
    class InplaceFunction;

    template <typename R, typename... Args, std::size_t Capacity>
    class InplaceFunction<R(Args...), Capacity> final {
        struct VTable final {
            R (*invoke)(void *storage, Args&&... args);
            void (*moveAndDestroy)(void *dst, void *src) noexcept;
            void (*destroy)(void *storage) noexcept;
        };

        template <typename F>
        static constexpr VTable vtableFor{
            [](void *storage, Args&&... args) -> R {
                return std::invoke(*std::launder(reinterpret_cast<F*>(storage)), std::forward<Args>(args)...);
            },
            [](void *dst, void *src) noexcept {
                F *srcF = std::launder(reinterpret_cast<F*>(src));
                std::construct_at(reinterpret_cast<F*>(dst), std::move(*srcF));
                std::destroy_at(srcF);
            },
            [](void *storage) noexcept {
                std::destroy_at(std::launder(reinterpret_cast<F*>(storage)));
            }
        };

    public:
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): _storage is where the callable is constructed, zeroing it would cost every function a memset
        InplaceFunction() noexcept = default;
        // NOLINTNEXTLINE(cppcoreguidelines-explicit-constructor): like std::function, nullptr is an empty function
        InplaceFunction(std::nullptr_t) noexcept {}

        template <typename Fn, typename F = std::decay_t<Fn>>
        requires (!std::is_same_v<F, InplaceFunction> && std::is_invocable_r_v<R, F&, Args...>)
        // NOLINTNEXTLINE(cppcoreguidelines-explicit-constructor): like std::function, a callable converts to it
        InplaceFunction(Fn &&fn) noexcept(std::is_nothrow_constructible_v<F, Fn&&>) : _vtable(&vtableFor<F>) {
            static_assert(sizeof(F) <= Capacity, "Callable too large for InplaceFunction, reduce the captured state or increase the capacity");
            static_assert(alignof(F) <= alignof(std::max_align_t), "Callable over-aligned for InplaceFunction");
            static_assert(std::is_nothrow_move_constructible_v<F>, "InplaceFunction requires nothrow move constructible callables");
            std::construct_at(reinterpret_cast<F*>(_storage.data()), std::forward<Fn>(fn));
        }

        InplaceFunction(InplaceFunction &&o) noexcept : _vtable(o._vtable) {
            if(_vtable != nullptr) {
                _vtable->moveAndDestroy(_storage.data(), o._storage.data());
                o._vtable = nullptr;
            }
        }

        InplaceFunction& operator=(InplaceFunction &&o) noexcept {
            if(this != &o) {
                reset();
                if(o._vtable != nullptr) {
                    o._vtable->moveAndDestroy(_storage.data(), o._storage.data());
                    _vtable = o._vtable;
                    o._vtable = nullptr;
                }
            }
            return *this;
        }

        InplaceFunction& operator=(std::nullptr_t) noexcept {
            reset();
            return *this;
        }

        InplaceFunction(InplaceFunction const &) = delete;
        InplaceFunction& operator=(InplaceFunction const &) = delete;

        ~InplaceFunction() {
            reset();
        }

        R operator()(Args... args) {
            if(_vtable == nullptr) [[unlikely]] {
                std::terminate();
            }
            return _vtable->invoke(_storage.data(), std::forward<Args>(args)...);
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return _vtable != nullptr;
        }

        void reset() noexcept {
            if(_vtable != nullptr) {
                _vtable->destroy(_storage.data());
                _vtable = nullptr;
            }
        }

    private:
        VTable const *_vtable{};
        alignas(std::max_align_t) std::array<std::byte, Capacity> _storage;
    };
}
