#pragma once

#include <cstdint>
#include <exception>
#include <type_traits>
#include <memory>
#include <utility>
#include <ichor/Defines.h>
#include <ichor/ConstevalHash.h>
#include <ichor/stl/CompilerSpecific.h>

#ifdef ICHOR_ENABLE_INTERNAL_STL_DEBUGGING
#include <atomic>
#endif

namespace Ichor::v1 {

    template <typename T>
    class ReferenceCountedPointer;

    namespace Detail {
        template<typename T, typename = void>
        struct IsComplete : std::false_type {};

        template<typename T>
        struct IsComplete<T, std::void_t<decltype(sizeof(T))>> : std::true_type {};

        template<bool Complete, typename T, typename... U>
        struct ConstructibleHelperSelector;

        template<typename T, typename... U>
        struct ConstructibleHelperSelector<true, T, U...> : std::bool_constant<std::is_constructible_v<T, U...>> {};

        template<typename T, typename... U>
        struct ConstructibleHelperSelector<false, T, U...> : std::true_type {};

        template<typename T, typename... U>
        struct ConstructibleHelper : ConstructibleHelperSelector<IsComplete<T>::value, T, U...> {};

        template <typename T>
        struct IsReferenceCountedPointer : std::false_type {};

        template <typename T>
        struct IsReferenceCountedPointer<ReferenceCountedPointer<T>> : std::true_type {};
    }

    template <typename T, typename... U>
    concept Constructible = Detail::ConstructibleHelper<T, U...>::value;

    /// A ReferenceCountedPointer<U> can be converted into a ReferenceCountedPointer<T> if a U* can be converted into a T*, e.g. when T is a base of U.
    template <typename U, typename T>
    concept PointerConvertible = std::is_convertible_v<U*, T*>;


#ifdef ICHOR_ENABLE_INTERNAL_STL_DEBUGGING
    extern std::atomic<uint64_t> _rfpCounter;
#define ICHOR_RFP_ID _id,
#else
#define ICHOR_RFP_ID
#endif



    namespace Detail {
        /// Control block, only holds the reference count. Destroying it destroys the owned object.
        struct [[nodiscard]] ReferenceCountedPointerBase {
            ReferenceCountedPointerBase(const ReferenceCountedPointerBase &) = delete;
            ReferenceCountedPointerBase(ReferenceCountedPointerBase &&) = delete;
            ReferenceCountedPointerBase &operator=(const ReferenceCountedPointerBase &) = delete;
            ReferenceCountedPointerBase &operator=(ReferenceCountedPointerBase &&) = delete;
            constexpr virtual ~ReferenceCountedPointerBase() noexcept = default;
            uint64_t useCount{1};

        protected:
            constexpr ReferenceCountedPointerBase() noexcept = default;
        };

        /// Control block for an object that was allocated separately and needs a deleter.
        template <typename U, typename DeleterType>
        struct [[nodiscard]] ReferenceCountedPointerDeleter final : public ReferenceCountedPointerBase {
            constexpr explicit ReferenceCountedPointerDeleter(U *p, DeleterType fn) noexcept(std::is_nothrow_move_constructible_v<DeleterType>) : ptr(p), deleteFn(std::move(fn)) {
            }

            constexpr ~ReferenceCountedPointerDeleter() noexcept final {
                deleteFn(ptr);
            }

            U *ptr;
            DeleterType deleteFn;
        };

        /// Control block that contains the object itself, saving an allocation.
        template <typename T>
        struct [[nodiscard]] ReferenceCountedPointerInline final : public ReferenceCountedPointerBase {
            template <typename... Args>
            constexpr explicit ReferenceCountedPointerInline(Args&&... args) : value(std::forward<Args>(args)...) {
            }

            constexpr ~ReferenceCountedPointerInline() noexcept final = default;

            T value;
        };

        struct DefaultReferenceCountedDeleter final {
            template <typename U>
            constexpr void operator()(U *p) const noexcept {
                delete p;
            }
        };
    }

    /// Non-atomic reference counted pointer
    template <typename T>
    class [[nodiscard]] ReferenceCountedPointer final {
    public:
        constexpr ReferenceCountedPointer() noexcept = default;
        constexpr ReferenceCountedPointer(decltype(nullptr)) noexcept {}
        constexpr ReferenceCountedPointer(const ReferenceCountedPointer &o) noexcept : _ptr(o._ptr), _block(o._block) {
            increment();
        }
        template <typename U> requires (!std::is_same_v<T, U> && PointerConvertible<U, T>)
        constexpr ReferenceCountedPointer(const ReferenceCountedPointer<U> &o) noexcept : _ptr(o._ptr), _block(o._block) {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}>(const ReferenceCountedPointer<{}> &o) {} {}", typeName<T>(), typeName<U>(), ICHOR_RFP_ID _block == nullptr);
            increment();
        }
        constexpr ReferenceCountedPointer(ReferenceCountedPointer &&o) noexcept : _ptr(o._ptr), _block(o._block) {
            o._ptr = nullptr;
            o._block = nullptr;
        }
        template <typename U> requires (!std::is_same_v<T, U> && PointerConvertible<U, T>)
        constexpr ReferenceCountedPointer(ReferenceCountedPointer<U> &&o) noexcept : _ptr(o._ptr), _block(o._block) {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}>(ReferenceCountedPointer<{}> &&o) {} {}", typeName<T>(), typeName<U>(), ICHOR_RFP_ID _block == nullptr);
            o._ptr = nullptr;
            o._block = nullptr;
        }

        /// Takes ownership of p. A nullptr results in an empty ReferenceCountedPointer.
        template <typename U> requires PointerConvertible<U, T>
        explicit constexpr ReferenceCountedPointer(U* p) {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}>({}* p) {} {}", typeName<T>(), typeName<U>(), ICHOR_RFP_ID p == nullptr);
            adopt(p, Detail::DefaultReferenceCountedDeleter{});
        }

        /// Constructs a T in place, using a single allocation for both the reference count and the T.
        template <typename... U> requires (Constructible<T, U...> && !(sizeof...(U) == 1 && (Detail::IsReferenceCountedPointer<std::remove_cvref_t<U>>::value && ...)))
        ICHOR_CXX23_CONSTEXPR explicit ReferenceCountedPointer(U&&... args) : ReferenceCountedPointer(std::in_place, std::forward<U>(args)...) {
        }

        /// Constructs a T in place, using a single allocation for both the reference count and the T. Also works without arguments, unlike the constructor above.
        template <typename... U> requires Constructible<T, U...>
        ICHOR_CXX23_CONSTEXPR explicit ReferenceCountedPointer(std::in_place_t, U&&... args) {
            if constexpr (std::is_same_v<T, bool>) {
                static_assert(sizeof...(U) == 1, "ReferenceCountedPointer<bool> expects a single bool value");
                static_assert((std::is_same_v<bool, std::remove_cvref_t<U>> && ...), "ReferenceCountedPointer<bool> expects bool-compatible argument");
            }
            auto *block = new Detail::ReferenceCountedPointerInline<T>(std::forward<U>(args)...);
            _block = block;
            _ptr = std::addressof(block->value);
            if constexpr (std::is_same_v<T, bool>) {
                INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}>(U&&... args) {} {} bool! {}", typeName<T>(), ICHOR_RFP_ID _block == nullptr, *_ptr);
            } else {
                INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}>(U&&... args) {} {} {}", typeName<T>(), ICHOR_RFP_ID _block == nullptr, sizeof(T));
            }
        }

        /// Takes ownership of the unique_ptr. An empty unique_ptr results in an empty ReferenceCountedPointer.
        template <typename U, typename UniqueDeleter> requires PointerConvertible<U, T>
        ICHOR_CXX23_CONSTEXPR ReferenceCountedPointer(std::unique_ptr<U, UniqueDeleter> unique) {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}>(unique) {} {}", typeName<T>(), ICHOR_RFP_ID unique == nullptr);
            adoptUnique(std::move(unique));
        }

        constexpr ~ReferenceCountedPointer() noexcept {
            INTERNAL_STL_DEBUG("~ReferenceCountedPointer<{}>() {} {}", typeName<T>(), ICHOR_RFP_ID _block == nullptr);
            decrement();
        }

        constexpr ReferenceCountedPointer& operator=(const ReferenceCountedPointer &o) noexcept {
            // o might be owned by the object we're releasing (e.g. node = node->next), so copy and increment before decrementing, o may be gone afterwards.
            auto *ptr = o._ptr;
            auto *block = o._block;
            o.increment();
            decrement();
            _ptr = ptr;
            _block = block;

            return *this;
        }

        template <typename U> requires (!std::is_same_v<T, U> && PointerConvertible<U, T>)
        constexpr ReferenceCountedPointer& operator=(const ReferenceCountedPointer<U> &o) noexcept {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator=(const ReferenceCountedPointer<{}> &o) {} {}", typeName<T>(), typeName<U>(), ICHOR_RFP_ID _block == nullptr);
            T *ptr = o._ptr;
            auto *block = o._block;
            o.increment();
            decrement();
            _ptr = ptr;
            _block = block;

            return *this;
        }

        constexpr ReferenceCountedPointer& operator=(ReferenceCountedPointer &&o) noexcept {
            if(this == &o) [[unlikely]] {
                return *this;
            }

            // Take ownership before releasing, o might be owned by the object we're releasing.
            auto *ptr = o._ptr;
            auto *block = o._block;
            o._ptr = nullptr;
            o._block = nullptr;
            decrement();
            _ptr = ptr;
            _block = block;

            return *this;
        }

        template <typename U> requires (!std::is_same_v<T, U> && PointerConvertible<U, T>)
        constexpr ReferenceCountedPointer& operator=(ReferenceCountedPointer<U> &&o) noexcept {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator=(ReferenceCountedPointer<{}> &&o) {} {}", typeName<T>(), typeName<U>(), ICHOR_RFP_ID _block == nullptr);
            T *ptr = o._ptr;
            auto *block = o._block;
            o._ptr = nullptr;
            o._block = nullptr;
            decrement();
            _ptr = ptr;
            _block = block;

            return *this;
        }

        /// Takes ownership of p. A nullptr results in an empty ReferenceCountedPointer.
        template <typename U> requires PointerConvertible<U, T>
        constexpr ReferenceCountedPointer& operator=(U *p) {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator=({}* p) {} {}", typeName<T>(), typeName<U>(), ICHOR_RFP_ID _block == nullptr);
            ReferenceCountedPointer tmp{p};
            swap(tmp);

            return *this;
        }

        /// Takes ownership of the unique_ptr. An empty unique_ptr results in an empty ReferenceCountedPointer.
        template <typename U, typename UniqueDeleter> requires PointerConvertible<U, T>
        constexpr ReferenceCountedPointer& operator=(std::unique_ptr<U, UniqueDeleter> unique) {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator=(unique) {} {}", typeName<T>(), ICHOR_RFP_ID _block == nullptr);
            ReferenceCountedPointer tmp{std::move(unique)};
            swap(tmp);

            return *this;
        }

        constexpr ReferenceCountedPointer& operator=(decltype(nullptr)) noexcept {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator=(nullptr) {} {}", typeName<T>(), ICHOR_RFP_ID _block == nullptr);
            decrement();
            _ptr = nullptr;
            _block = nullptr;

            return *this;
        }

        [[nodiscard]] constexpr T& operator*() const noexcept ICHOR_LIFETIME_BOUND {
            if constexpr (std::is_same_v<T, bool>) {
                INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator*() {} {} bool! {}", typeName<T>(), ICHOR_RFP_ID _block == nullptr, *_ptr);
            } else {
                INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator*() {} {}", typeName<T>(), ICHOR_RFP_ID _block == nullptr);
            }
            if constexpr (DO_INTERNAL_STL_DEBUG || DO_HARDENING) {
                if (_ptr == nullptr) [[unlikely]] {
                    std::terminate();
                }
            }
            return *_ptr;
        }

        [[nodiscard]] constexpr T* operator->() const noexcept ICHOR_LIFETIME_BOUND {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator->() {} {}", typeName<T>(), ICHOR_RFP_ID _block == nullptr);
            if constexpr (DO_INTERNAL_STL_DEBUG || DO_HARDENING) {
                if (_ptr == nullptr) [[unlikely]] {
                    std::terminate();
                }
            }
            return _ptr;
        }

        // TODO enabling this function causes function overloading to fail for ReferenceCountedPointer<bool>(const &ReferenceCountedPointer<bool> o).
        // causing it to construct a new bool from a ReferenceCountedPointer<bool> instead of the underlying bool.
        // Maybe try https://www.artima.com/articles/the-safe-bool-idiom ?
//        [[nodiscard]] explicit operator bool() const noexcept {
//            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator bool() {} {}", typeName<T>(), ICHOR_RFP_ID _block == nullptr);
//            return _block != nullptr;
//        }

        [[nodiscard]] constexpr bool operator==(decltype(nullptr)) const noexcept {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator==(nullptr) {} {}", typeName<T>(), ICHOR_RFP_ID _block == nullptr);
            return _block == nullptr;
        }

        template <typename U>
        [[nodiscard]] constexpr bool operator==(const ReferenceCountedPointer<U> &o) const noexcept {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator==(const ReferenceCountedPointer<{}> &) {} {}", typeName<T>(), typeName<U>(), ICHOR_RFP_ID _block == nullptr);
            return _block == o._block;
        }

        [[nodiscard]] constexpr uint64_t use_count() const noexcept {
            if(_block == nullptr) {
                return 0;
            }

            return _block->useCount;
        }

        [[nodiscard]] constexpr bool has_value() const noexcept {
            return _block != nullptr;
        }

        [[nodiscard]] constexpr T* get() const noexcept ICHOR_LIFETIME_BOUND {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> get() {} {}", typeName<T>(), ICHOR_RFP_ID _block == nullptr);
            if constexpr (DO_INTERNAL_STL_DEBUG || DO_HARDENING) {
                if (_ptr == nullptr) [[unlikely]] {
                    std::terminate();
                }
            }
            return _ptr;
        }

        constexpr void swap(ReferenceCountedPointer<T> &o) noexcept {
            std::swap(_ptr, o._ptr);
            std::swap(_block, o._block);
        }

        friend constexpr void swap(ReferenceCountedPointer<T> &a, ReferenceCountedPointer<T> &b) noexcept {
            a.swap(b);
        }

    private:
        template <typename U, typename Deleter>
        constexpr void adopt(U *p, Deleter deleter) {
            if(p == nullptr) {
                return;
            }
            // Own p before allocating the control block, so that p gets cleaned up if the allocation throws.
            std::unique_ptr<U, Deleter> guard{p, deleter};
            adoptUnique(std::move(guard));
        }

        template <typename U, typename UniqueDeleter>
        constexpr void adoptUnique(std::unique_ptr<U, UniqueDeleter> unique) {
            if(unique == nullptr) {
                return;
            }
            auto *p = unique.get();
            auto deleteFn = [deleter = unique.get_deleter()](U *ptr) mutable noexcept { deleter(ptr); };
            _block = new Detail::ReferenceCountedPointerDeleter<U, decltype(deleteFn)>(p, std::move(deleteFn));
            _ptr = p;
            static_cast<void>(unique.release());
        }

        constexpr void increment() const noexcept {
            if(_block != nullptr) {
                _block->useCount++;
            }
        }

        constexpr void decrement() const noexcept {
            if(_block != nullptr) {
                _block->useCount--;
                if(_block->useCount == 0) {
                    delete _block;
                }
            }
        }

        // Pointer to the object, adjusted to T. Stored separately from the control block so that conversions between base and derived classes adjust the pointer correctly.
        T *_ptr{};
        Detail::ReferenceCountedPointerBase *_block{};
#ifdef ICHOR_ENABLE_INTERNAL_STL_DEBUGGING
        uint64_t _id{_rfpCounter.fetch_add(1, std::memory_order_relaxed)};
#endif

        template<typename>
        friend class ReferenceCountedPointer;
    };

    template <typename T, typename... Args>
    constexpr ReferenceCountedPointer<T> make_reference_counted(Args&&... args) {
        // in_place, otherwise calling this without arguments would select the default constructor and return an empty pointer.
        return ReferenceCountedPointer<T>(std::in_place, std::forward<Args>(args)...);
    }
}

#undef ICHOR_RFP_ID
