#pragma once

#include <memory_resource>
#include <ichor/ConstevalHash.h>

// Differs from std::any by explicitly requiring a memory_resource to allocate from
// Probably doesn't work in some situations where std::any would, as compiler support is missing.
namespace Ichor {

    struct bad_any_cast final : public std::bad_cast
    {
        bad_any_cast(std::pmr::memory_resource *rsrc, std::string_view type, std::string_view cast) : _error(rsrc) {
            _error = "Bad any_cast. Expected ";
            _error += type;
            _error += " but got request to cast to ";
            _error += cast;
        }

        [[nodiscard]] const char* what() const noexcept final { return _error.c_str(); }

        std::pmr::string _error;
    };

    using createValueFnPtr = void*(*)(std::pmr::memory_resource *, void*);
    using deleteValueFnPtr = void(*)(void*);

    struct any final {
        constexpr any() noexcept = default;

        any(const any& o) : _size(o._size), _rsrc(o._rsrc), _ptr(o._createFn(o._rsrc, o._ptr)), _typeHash(o._typeHash), _hasValue(o._hasValue), _createFn(o._createFn), _deleteFn(o._deleteFn), _typeName(o._typeName) {

        }

        any(any&& o) noexcept : _size(o._size), _rsrc(o._rsrc), _ptr(o._ptr), _typeHash(o._typeHash), _hasValue(o._hasValue), _createFn(o._createFn), _deleteFn(o._deleteFn), _typeName(o._typeName) {
            o._hasValue = false;
            o._ptr = nullptr;
        }

        any& operator=(const any& o) {
            if(this == &o) {
                return *this;
            }

            _size = o._size;
            _rsrc = o._rsrc;
            _ptr = o._createFn(o._rsrc, o._ptr);
            _typeHash = o._typeHash;
            _hasValue = o._hasValue;
            _createFn = o._createFn;
            _deleteFn = o._deleteFn;
            _typeName = o._typeName;

            return *this;
        }

        any& operator=(any&& o) noexcept {
            _size = o._size;
            _rsrc = o._rsrc;
            _ptr = o._ptr;
            _typeHash = o._typeHash;
            _hasValue = o._hasValue;
            _createFn = o._createFn;
            _deleteFn = o._deleteFn;
            _typeName = o._typeName;

            o._hasValue = false;
            o._ptr = nullptr;

            return *this;
        }

        ~any() {
            reset();
        }

        template <typename T, typename... Args>
        std::decay<T>& emplace(std::pmr::memory_resource *rsrc, Args&&... args)
        {
            static_assert(std::is_copy_constructible_v<T>, "Template argument must be copy constructible.");
            static_assert(!std::is_pointer_v<T>, "Template argument must not be a pointer");

            reset();

            _size = sizeof(T);
            _rsrc = rsrc;
            _ptr = new (_rsrc->allocate(sizeof(T))) T(std::forward<Args>(args)...);
            _typeHash = typeNameHash<std::remove_cvref_t<T>>();
            _hasValue = true;
            _createFn = [](std::pmr::memory_resource *rsrc, void *value) -> void* {
                return new (rsrc->allocate(sizeof(T))) T(*reinterpret_cast<T*>(value));
            };
            _deleteFn = [](void *value) {
                static_cast<T*>(value)->~T();
            };
            _typeName = typeName<std::remove_cvref_t<T>>();

            return *reinterpret_cast<std::decay<T>*>(_ptr);
        }

        void reset() {
            if(_hasValue) {
                _deleteFn(_ptr);
                _rsrc->deallocate(_ptr, _size);
                _hasValue = false;
            }
        }

        [[nodiscard]] bool has_value() const noexcept {
            return _hasValue;
        }

        [[nodiscard]] uint64_t type_hash() const noexcept {
            return _typeHash;
        }

        template<typename ValueType>
        ValueType any_cast()
        {
            using Up = typename std::remove_pointer_t<std::remove_cvref_t<ValueType>>;
            static_assert((std::is_reference_v<ValueType> || std::is_copy_constructible_v<ValueType>), "Template argument must be a reference or CopyConstructible type");
            static_assert(std::is_constructible_v<ValueType, Up&>, "Template argument must be constructible from an lvalue.");
            if(_typeHash == typeNameHash<Up>()) {
                return static_cast<ValueType>(*reinterpret_cast<Up*>(_ptr));
            }
            throw bad_any_cast{_rsrc, _typeName, typeName<Up>()};
        }

        template<typename ValueType>
        ValueType any_cast() const
        {
            using Up = typename std::remove_pointer_t<std::remove_cvref_t<ValueType>>;
            static_assert((std::is_reference_v<ValueType> || std::is_copy_constructible_v<ValueType>), "Template argument must be a reference or CopyConstructible type");
            static_assert(std::is_constructible_v<ValueType, const Up&>, "Template argument must be constructible from a const lvalue.");
            if(_typeHash == typeNameHash<Up>()) {
                return static_cast<ValueType>(*reinterpret_cast<Up*>(_ptr));
            }
            throw bad_any_cast{_rsrc, _typeName, typeName<Up>()};
        }

        template<typename ValueType>
        ValueType any_cast_ptr() const
        {
            using Up = typename std::remove_pointer_t<std::remove_cvref_t<ValueType>>;
            static_assert((std::is_pointer_v<ValueType> ), "Template argument must be a pointer type");
            auto comparison = typeNameHash<Up>();
            if(_typeHash == comparison) {
                return reinterpret_cast<ValueType>(_ptr);
            }
            throw bad_any_cast{_rsrc, _typeName, typeName<Up>()};
        }

    private:
        std::size_t _size{};
        std::pmr::memory_resource *_rsrc{};
        void* _ptr{};
        uint64_t _typeHash{};
        bool _hasValue{};
        createValueFnPtr _createFn{};
        deleteValueFnPtr _deleteFn{};
        std::string_view _typeName{};
    };

    template<typename ValueType>
    ValueType any_cast(const any& a)
    {
        return a.template any_cast<ValueType>();
    }

    template<typename ValueType>
    ValueType any_cast(any& a)
    {
        return a.template any_cast<ValueType>();
    }

    template<typename ValueType>
    ValueType any_cast(any const *a)
    {
        return a->template any_cast_ptr<ValueType>();
    }

    template <typename T, typename... Args>
    any make_any(std::pmr::memory_resource *rsrc, Args&&... args) {
        any a;
        a.template emplace<T, Args...>(rsrc, std::forward<Args>(args)...);
        return a;
    }

    template <typename T, typename... Args>
    any make_any(Args&&... args) {
        any a;
        a.template emplace<T, Args...>(std::pmr::get_default_resource(), std::forward<Args>(args)...);
        return a;
    }
}