#pragma once

#include <ichor/ConstevalHash.h>
#include <typeinfo>
#include <string>
#include <string_view>
#include <fmt/core.h>

// Differs from std::any by not needing RTTI (no typeid()) and still able to compare types
// e.g my_any_var.type_hash() == Ichor::typeNameHash<int>()
// Also supports to_string() on the underlying value T, if there exists an fmt::formatter<T>.
// Probably doesn't work in some situations where std::any would, as compiler support is missing.
namespace Ichor {

    struct bad_any_cast final : public std::bad_cast {
        bad_any_cast(std::string_view type, std::string_view cast) {
            _error = "Bad any_cast. Expected ";
            _error += type;
            _error += " but got request to cast to ";
            _error += cast;
        }

        [[nodiscard]] const char* what() const noexcept final { return _error.c_str(); }

        std::string _error;
    };

    using createValueFnPtr = void*(*)(void*) noexcept;
    using deleteValueFnPtr = void(*)(void*) noexcept;
    using formatValueFnPt = std::string(*)(void*) noexcept;

    /// Type-safe container for single values for any copy constructible type. Differs from std::any by not requiring RTTI for any operation, as well as supplying things like size, type_name and to_string() functions.
    /// By default, requires the type to have an existing fmt::formatter. Use Ichor::make_unformattable_any if it is not feasible to implement one.
    struct any final {
        any() noexcept = default;

        any(const any& o) : _size(o._size), _ptr(o._createFn(o._ptr)), _typeHash(o._typeHash), _hasValue(o._hasValue), _createFn(o._createFn), _deleteFn(o._deleteFn), _formatFn(o._formatFn), _typeName(o._typeName) {

        }

        any(any&& o) noexcept : _size(o._size), _ptr(o._ptr), _typeHash(o._typeHash), _hasValue(o._hasValue), _createFn(o._createFn), _deleteFn(o._deleteFn), _formatFn(o._formatFn), _typeName(o._typeName) {
            o._hasValue = false;
            o._ptr = nullptr;
        }

        any& operator=(const any& o) noexcept {
            if(this == &o) [[unlikely]] {
                return *this;
            }

            reset();

            _size = o._size;
            _ptr = o._createFn(o._ptr);
            _typeHash = o._typeHash;
            _hasValue = o._hasValue;
            _createFn = o._createFn;
            _deleteFn = o._deleteFn;
            _formatFn = o._formatFn;
            _typeName = o._typeName;

            return *this;
        }

        any& operator=(any&& o) noexcept {
            if(this == &o) [[unlikely]] {
                return *this;
            }

            reset();

            _size = o._size;
            _ptr = o._ptr;
            _typeHash = o._typeHash;
            _hasValue = o._hasValue;
            _createFn = o._createFn;
            _deleteFn = o._deleteFn;
            _formatFn = o._formatFn;
            _typeName = o._typeName;

            o._hasValue = false;
            o._ptr = nullptr;

            return *this;
        }

        ~any() noexcept {
            reset();
        }

        bool operator==(const any& other) const noexcept
        {
            return _typeHash == other._typeHash && _hasValue && _ptr == other._ptr;
        }

        template <typename T, typename... Args>
        std::decay<T>& emplace(Args&&... args) noexcept
        {
            static_assert(std::is_copy_constructible_v<T>, "Template argument must be copy constructible.");
            static_assert(!std::is_pointer_v<T>, "Template argument must not be a pointer");

            reset();

            _size = sizeof(T);
            _ptr = new T(std::forward<Args>(args)...);
            _typeHash = typeNameHash<std::remove_cvref_t<T>>();
            _hasValue = true;
            _createFn = [](void *value) noexcept -> void* {
                return new T(*reinterpret_cast<T*>(value));
            };
            _deleteFn = [](void *value) noexcept {
                delete static_cast<T*>(value);
            };
            _formatFn = [](void *value) noexcept {
                return fmt::format("{}", *static_cast<T*>(value));
            };
            _typeName = typeName<std::remove_cvref_t<T>>();

            return *reinterpret_cast<std::decay<T>*>(_ptr);
        }

        template <typename T, typename... Args>
        std::decay<T>& emplace_unformattable(Args&&... args) noexcept
        {
            static_assert(std::is_copy_constructible_v<T>, "Template argument must be copy constructible.");
            static_assert(!std::is_pointer_v<T>, "Template argument must not be a pointer");

            reset();

            _size = sizeof(T);
            _ptr = new T(std::forward<Args>(args)...);
            _typeHash = typeNameHash<std::remove_cvref_t<T>>();
            _hasValue = true;
            _createFn = [](void *value) noexcept -> void* {
                return new T(*reinterpret_cast<T*>(value));
            };
            _deleteFn = [](void *value) noexcept {
                delete static_cast<T*>(value);
            };
            _typeName = typeName<std::remove_cvref_t<T>>();

            return *reinterpret_cast<std::decay<T>*>(_ptr);
        }

        void reset() noexcept {
            if(_hasValue) {
                _deleteFn(_ptr);
                _hasValue = false;
                _ptr = nullptr;
            }
        }

        [[nodiscard]] std::string to_string() const noexcept {
            if(_formatFn == nullptr || _ptr == nullptr) {
                return "Unprintable value";
            }

            return _formatFn(_ptr);
        }

        [[nodiscard]] bool has_value() const noexcept {
            return _hasValue;
        }

        [[nodiscard]] uint64_t type_hash() const noexcept {
            return _typeHash;
        }

        [[nodiscard]] std::string_view type_name() const noexcept {
            return _typeName;
        }

        [[nodiscard]] std::size_t get_size() const noexcept {
            return _size;
        }

        template<typename ValueType>
        [[nodiscard]] ValueType any_cast()
        {
            using Up = typename std::remove_pointer_t<std::remove_cvref_t<ValueType>>;
            static_assert((std::is_reference_v<ValueType> || std::is_copy_constructible_v<ValueType>), "Template argument must be a reference or CopyConstructible type");
            static_assert(std::is_constructible_v<ValueType, Up&>, "Template argument must be constructible from an lvalue.");
            if(_hasValue && _typeHash == typeNameHash<Up>()) {
                return static_cast<ValueType>(*reinterpret_cast<Up*>(_ptr));
            }
            throw bad_any_cast{_typeName, typeName<Up>()};
        }

        template<typename ValueType>
        [[nodiscard]] ValueType any_cast() const
        {
            using Up = typename std::remove_pointer_t<std::remove_cvref_t<ValueType>>;
            static_assert((std::is_reference_v<ValueType> || std::is_copy_constructible_v<ValueType>), "Template argument must be a reference or CopyConstructible type");
            static_assert(std::is_constructible_v<ValueType, const Up&>, "Template argument must be constructible from a const lvalue.");
            if(_hasValue && _typeHash == typeNameHash<Up>()) {
                return static_cast<ValueType>(*reinterpret_cast<Up*>(_ptr));
            }
            throw bad_any_cast{_typeName, typeName<Up>()};
        }

        template<typename ValueType>
        [[nodiscard]] ValueType any_cast_ptr() const
        {
            using Up = typename std::remove_pointer_t<std::remove_cvref_t<ValueType>>;
            static_assert((std::is_pointer_v<ValueType> ), "Template argument must be a pointer type");
            auto comparison = typeNameHash<Up>();
            if(_hasValue && _typeHash == comparison) {
                return reinterpret_cast<ValueType>(_ptr);
            }
            throw bad_any_cast{_typeName, typeName<Up>()};
        }

    private:
        std::size_t _size{};
        void* _ptr{};
        uint64_t _typeHash{};
        bool _hasValue{};
        createValueFnPtr _createFn{};
        deleteValueFnPtr _deleteFn{};
        formatValueFnPt _formatFn{};
        std::string_view _typeName{};
    };

    template<typename ValueType>
    [[nodiscard]] ValueType any_cast(const any& a)
    {
        return a.template any_cast<ValueType>();
    }

    template<typename ValueType>
    [[nodiscard]] ValueType any_cast(any& a)
    {
        return a.template any_cast<ValueType>();
    }

    template<typename ValueType>
    [[nodiscard]] ValueType any_cast(any const *a)
    {
        return a->template any_cast_ptr<ValueType>();
    }

    template <typename T, typename... Args>
    [[nodiscard]] any make_any(Args&&... args) noexcept {
        any a;
        a.template emplace<T, Args...>(std::forward<Args>(args)...);
        return a;
    }

    template <typename T, typename... Args>
    [[nodiscard]] any make_unformattable_any(Args&&... args) noexcept {
        any a;
        a.template emplace_unformattable<T, Args...>(std::forward<Args>(args)...);
        return a;
    }
}
