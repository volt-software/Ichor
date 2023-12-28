#pragma once

#include <cstdint>
#include <type_traits>
#include <memory>
#include <atomic>
#include <ichor/stl/NeverAlwaysNull.h>
#include <ichor/Common.h>

namespace Ichor {

    template <typename T>
    class ReferenceCountedPointer;

    template <typename T, typename... U>
    concept Constructible = std::is_constructible_v<T, U...>;


#ifdef ICHOR_ENABLE_INTERNAL_STL_DEBUGGING
    extern std::atomic<uint64_t> _rfpCounter;
#define RFP_ID _id,
#else
#define RFP_ID
#endif



    namespace Detail {
        struct [[nodiscard]] ReferenceCountedPointerBase {
            ReferenceCountedPointerBase(const ReferenceCountedPointerBase &) = delete;
            ReferenceCountedPointerBase(ReferenceCountedPointerBase &&) = default;
            ReferenceCountedPointerBase &operator=(const ReferenceCountedPointerBase &) = delete;
            ReferenceCountedPointerBase &operator=(ReferenceCountedPointerBase &&) = default;
            virtual ~ReferenceCountedPointerBase() noexcept = default;
            NeverNull<void*> ptr;
            uint64_t useCount{};

        protected:
            ReferenceCountedPointerBase(void* _ptr, uint64_t _useCount) noexcept : ptr(_ptr), useCount(_useCount) {
            }
        };

        template <typename DeleterType>
        struct [[nodiscard]] ReferenceCountedPointerDeleter final : public ReferenceCountedPointerBase {
            ReferenceCountedPointerDeleter(const ReferenceCountedPointerDeleter &) = delete;
            ReferenceCountedPointerDeleter(ReferenceCountedPointerDeleter &&) = default;
            ReferenceCountedPointerDeleter &operator=(const ReferenceCountedPointerDeleter &) = delete;
            ReferenceCountedPointerDeleter &operator=(ReferenceCountedPointerDeleter &&) = default;

            template<typename U>
            explicit ReferenceCountedPointerDeleter(U *p, DeleterType fn) noexcept : ReferenceCountedPointerBase(p, 1), deleteFn(fn) {

            }

            ~ReferenceCountedPointerDeleter() noexcept final {
                deleteFn(ptr);
            }

            DeleterType deleteFn;
        };
    }

    /// Non-atomic reference counted pointer
    template <typename T>
    class [[nodiscard]] ReferenceCountedPointer final {
    public:
        constexpr ReferenceCountedPointer() noexcept = default;
        ReferenceCountedPointer(const ReferenceCountedPointer &o) noexcept : _ptr(o._ptr) {
            _ptr->useCount++;
        }
        template <typename U> requires Constructible<T, U>
        ReferenceCountedPointer(const ReferenceCountedPointer<U> &o) noexcept : _ptr(o._ptr) {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}>(const ReferenceCountedPointer<{}> &o) {} {}", typeName<T>(), typeName<U>(), RFP_ID _ptr == nullptr);
            _ptr->useCount++;
        }
        ReferenceCountedPointer(ReferenceCountedPointer &&o) noexcept : _ptr(o._ptr) {
            o._ptr = nullptr;
        }
        template <typename U> requires Constructible<T, U>
        ReferenceCountedPointer(ReferenceCountedPointer<U> &&o) noexcept : _ptr(o._ptr) {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}>(ReferenceCountedPointer<{}> &&o) {} {}", typeName<T>(), typeName<U>(), RFP_ID _ptr == nullptr);
            o._ptr = nullptr;
        }

        explicit ReferenceCountedPointer(T* p) : _ptr(new Detail::ReferenceCountedPointerDeleter(p, [](void *ptr) { delete static_cast<T*>(ptr); })) {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}>(T* p) {} {}", typeName<T>(), RFP_ID _ptr == nullptr);
        }

        template <typename U> requires Constructible<T, U>
        explicit ReferenceCountedPointer(U* p) : _ptr(new Detail::ReferenceCountedPointerDeleter(p, [](void *ptr) { delete static_cast<U*>(ptr); })) {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}>({}* p) {} {}", typeName<T>(), typeName<U>(), RFP_ID _ptr == nullptr);
        }
        template <typename... U> requires Constructible<T, U...>
        explicit ReferenceCountedPointer(U&&... args) : _ptr(new Detail::ReferenceCountedPointerDeleter(new T(std::forward<U>(args)...), [](void *ptr) { delete static_cast<T*>(ptr); })) {
            if constexpr (std::is_same_v<T, bool>) {
                static_assert(std::is_same_v<T, U...>);
                INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}>(U&&... args) {} {} bool! {}", typeName<T>(), RFP_ID _ptr == nullptr, *static_cast<T*>(_ptr->ptr.get()));
            } else {
                INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}>(U&&... args) {} {} {}", typeName<T>(), RFP_ID _ptr == nullptr, sizeof(T));
            }
        }
        template <typename UniqueDeleter>
        ReferenceCountedPointer(std::unique_ptr<T, UniqueDeleter> unique) : _ptr(new Detail::ReferenceCountedPointerDeleter(unique.get(), [deleter = unique.get_deleter()](void *ptr) { deleter(static_cast<T*>(ptr)); })) {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}>(unique) {} {}", typeName<T>(), RFP_ID _ptr == nullptr);
            unique.release();
        }

        ~ReferenceCountedPointer() noexcept {
            INTERNAL_STL_DEBUG("~ReferenceCountedPointer<{}>() {} {}", typeName<T>(), RFP_ID _ptr == nullptr);
            decrement();
        }

        ReferenceCountedPointer& operator=(const ReferenceCountedPointer &o) noexcept {
            if(this == &o) [[unlikely]] {
                return *this;
            }

            decrement();
            _ptr = o._ptr;
            _ptr->useCount++;

            return *this;
        }

        template <typename U> requires Constructible<T, U>
        ReferenceCountedPointer& operator=(const ReferenceCountedPointer<U> &o) noexcept {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator=(const ReferenceCountedPointer<{}> &o) {} {}", typeName<T>(), typeName<U>(), RFP_ID _ptr == nullptr);
            decrement();
            _ptr = o._ptr;
            _ptr->useCount++;

            return *this;
        }

        ReferenceCountedPointer& operator=(ReferenceCountedPointer &&o) noexcept {
            if(this == &o) [[unlikely]] {
                return *this;
            }

            decrement();
            _ptr = o._ptr;
            o._ptr = nullptr;

            return *this;
        }

        template <typename U> requires Constructible<T, U>
        ReferenceCountedPointer& operator=(ReferenceCountedPointer<U> &&o) noexcept {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator=(ReferenceCountedPointer<{}> &&o) {} {}", typeName<T>(), typeName<U>(), RFP_ID _ptr == nullptr);
            decrement();
            _ptr = o._ptr;
            o._ptr = nullptr;

            return *this;
        }

        template <typename U> requires Constructible<T, U>
        ReferenceCountedPointer& operator=(U *p) noexcept {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator=({}* p) {} {}", typeName<T>(), typeName<U>(), RFP_ID _ptr == nullptr);
            decrement();
            _ptr = new Detail::ReferenceCountedPointerDeleter(p, [](void *ptr) { delete static_cast<U*>(ptr); });

            return *this;
        }

        template <typename UniqueDeleter>
        ReferenceCountedPointer& operator=(std::unique_ptr<T, UniqueDeleter> unique) noexcept {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator=(unique) {} {}", typeName<T>(), RFP_ID _ptr == nullptr);
            decrement();
            _ptr = new Detail::ReferenceCountedPointerDeleter(unique.get(), [deleter = unique.get_deleter()](void *ptr) { deleter(static_cast<T*>(ptr)); });
            unique.release();

            return *this;
        }

        ReferenceCountedPointer& operator=(decltype(nullptr)) noexcept {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator=(nullptr) {} {}", typeName<T>(), RFP_ID _ptr == nullptr);
            decrement();
            _ptr = nullptr;

            return *this;
        }

        [[nodiscard]] T& operator*() const noexcept {
            if constexpr (std::is_same_v<T, bool>) {
                INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator*() {} {} bool! {}", typeName<T>(), RFP_ID _ptr == nullptr, *static_cast<T*>(_ptr->ptr.get()));
            } else {
                INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator*() {} {}", typeName<T>(), RFP_ID _ptr == nullptr);
            }
            if constexpr (DO_INTERNAL_STL_DEBUG || DO_HARDENING) {
                if (_ptr == nullptr) [[unlikely]] {
                    std::terminate();
                }
            }
            return *static_cast<T*>(_ptr->ptr.get());
        }

        [[nodiscard]] T* operator->() const noexcept {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator->() {} {}", typeName<T>(), RFP_ID _ptr == nullptr);
        if constexpr (DO_INTERNAL_STL_DEBUG || DO_HARDENING) {
            if (_ptr == nullptr) [[unlikely]] {
                std::terminate();
            }
        }
            return static_cast<T*>(_ptr->ptr.get());
        }

        // TODO enabling this function causes function overloading to fail for ReferenceCountedPointer<bool>(const &ReferenceCountedPointer<bool> o).
        // causing it to construct a new bool from a ReferenceCountedPointer<bool> instead of the underlying bool.
        // Maybe try https://www.artima.com/articles/the-safe-bool-idiom ?
//        [[nodiscard]] explicit operator bool() const noexcept {
//            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator bool() {} {}", typeName<T>(), RFP_ID _ptr == nullptr);
//            return _ptr != nullptr;
//        }

        [[nodiscard]] bool operator==(decltype(nullptr)) const noexcept {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator==(nullptr) {} {}", typeName<T>(), RFP_ID _ptr == nullptr);
            return _ptr == nullptr;
        }

        template <typename U>
        [[nodiscard]] bool operator==(const ReferenceCountedPointer<U> &o) const noexcept {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> operator==(const ReferenceCountedPointer<{}> &) {} {}", typeName<T>(), typeName<U>(), RFP_ID _ptr == nullptr);
            return _ptr == o._ptr;
        }

        [[nodiscard]] uint64_t use_count() const noexcept {
            if(_ptr == nullptr) {
                return 0;
            }

            return _ptr->useCount;
        }

        [[nodiscard]] bool has_value() const noexcept {
            return _ptr != nullptr;
        }

        [[nodiscard]] T* get() const noexcept {
            INTERNAL_STL_DEBUG("ReferenceCountedPointer<{}> get() {} {}", typeName<T>(), RFP_ID _ptr == nullptr);
            if constexpr (DO_INTERNAL_STL_DEBUG || DO_HARDENING) {
                if (_ptr == nullptr) [[unlikely]] {
                    std::terminate();
                }
            }
            return static_cast<T*>(_ptr->ptr.get());
        }

        void swap(ReferenceCountedPointer<T> &o) noexcept {
            auto *ptr = _ptr;
            _ptr = o._ptr;
            o._ptr = ptr;
        }

    private:
        void decrement() const noexcept {
            if(_ptr != nullptr) {
                _ptr->useCount--;
                if(_ptr->useCount == 0) {
                    delete _ptr;
                }
            }
        }

        Detail::ReferenceCountedPointerBase *_ptr{};
#ifdef ICHOR_ENABLE_INTERNAL_STL_DEBUGGING
        uint64_t _id{_rfpCounter.fetch_add(1, std::memory_order_relaxed)};
#endif

        template<typename>
        friend class ReferenceCountedPointer;
    };

    template <typename T, typename... Args>
    ReferenceCountedPointer<T> make_reference_counted(Args&&... args) {
        return ReferenceCountedPointer<T>(std::forward<Args>(args)...);
    }
}

namespace std {
    template<typename T>
    inline void swap(Ichor::ReferenceCountedPointer<T>& a, Ichor::ReferenceCountedPointer<T>& b) noexcept {
        a.swap(b);
    }
}
