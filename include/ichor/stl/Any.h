#pragma once

#include <ichor/ConstevalHash.h>
#include <typeinfo>
#include <array>
#include <string>
#include <string_view>
#include <fmt/core.h>
#include <exception>

// Differs from std::any by not needing RTTI (no typeid()) and still able to compare types
// e.g my_any_var.type_hash() == Ichor::typeNameHash<int>()
// Also supports to_string() on the underlying value T, if there exists an fmt::formatter<T>.
// Probably doesn't work in some situations where std::any would, as compiler support is missing.
namespace Ichor {

    struct bad_any_cast final : public std::bad_cast {
        bad_any_cast(std::string_view type, std::string_view cast) : _error(fmt::format("Bad any_cast. Expected {} but got request to cast to {}", type, cast)) {
        }

        [[nodiscard]] const char* what() const noexcept final { return _error.c_str(); }

        std::string _error;
    };

    enum class any_op {
        CLONE,
        DELETE_, // windows header annoyingly defines DELETE as a macro.
        MOVE
    };
    constexpr std::size_t bufferTypeSize = 16;
    using bufferType = std::array<std::byte, bufferTypeSize>;
    using opFnPtr = void*(*)(any_op, bufferType &, void*) noexcept;
    using formatValueFnPtr = std::string(*)(void*) noexcept;

    /// Type-safe container for single values for any copy constructible type. Differs from std::any by not requiring RTTI for any operation, as well as supplying things like size, type_name and to_string() functions.
    /// By default, requires the type to have an existing fmt::formatter. Use Ichor::make_unformattable_any if it is not feasible to implement one.
    struct any final {
        any() noexcept = default;

        any(const any& o) : _size(o._size), _typeHash(o._typeHash), _opFn(o._opFn), _formatFn(o._formatFn), _typeName(o._typeName) {
            if(o._ptr != nullptr) {
                _ptr = o._opFn(any_op::CLONE, _buffer, o._ptr);
            }
        }


        any(any&& o) noexcept : _size(o._size), _typeHash(o._typeHash), _opFn(o._opFn), _formatFn(o._formatFn), _typeName(o._typeName) {
            if(o._ptr == nullptr) {
                return;
            }

            if(o._ptr == reinterpret_cast<void*>(&o._buffer)) {
                _ptr = _opFn(any_op::MOVE, _buffer, o._ptr);
            } else {
                _ptr = o._ptr;
            }
            o._ptr = nullptr;
            o.reset();
        }

        any& operator=(const any& o) noexcept {
            if(this == &o) [[unlikely]] {
                return *this;
            }

            reset();

            _size = o._size;
            if(o._ptr != nullptr) {
                _ptr = o._opFn(any_op::CLONE, _buffer, o._ptr);
            }
            _typeHash = o._typeHash;
            _opFn = o._opFn;
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
            if(o._ptr == reinterpret_cast<void*>(&o._buffer)) {
                _ptr = o._opFn(any_op::MOVE, _buffer, o._ptr);
            } else {
                _ptr = o._ptr;
            }
            _typeHash = o._typeHash;
            _opFn = o._opFn;
            _formatFn = o._formatFn;
            _typeName = o._typeName;

            o._ptr = nullptr;

            return *this;
        }

        ~any() noexcept {
            reset();
        }

        bool operator==(const any& other) const noexcept {
            return _typeHash == other._typeHash && _ptr != nullptr && _ptr == other._ptr;
        }

        template <bool FORMATTABLE, typename T, typename... Args>
        std::decay<T>& emplace(Args&&... args) noexcept {
            static_assert(std::is_copy_constructible_v<T>, "Template argument must be copy constructible.");
            static_assert(!std::is_pointer_v<T>, "Template argument must not be a pointer");

            reset();

            _size = sizeof(T);
            if constexpr (sizeof(T) <= bufferTypeSize && std::is_nothrow_move_constructible_v<T>) {
                _ptr = new (_buffer.data()) T(std::forward<Args>(args)...);
            } else {
                _ptr = new T(std::forward<Args>(args)...);
            }
            _typeHash = typeNameHash<std::remove_cvref_t<T>>();
            _opFn = [](any_op op, bufferType &buffer, void *value) noexcept -> void* {
                if constexpr (sizeof(T) <= bufferTypeSize && std::is_nothrow_move_constructible_v<T>) {
                    if(op == any_op::CLONE) {
                        return new(buffer.data()) T(*reinterpret_cast<T *>(value));
                    } else if(op == any_op::DELETE_) {
                        if constexpr (!std::is_trivial_v<T>) {
                            static_cast<T *>(value)->~T();
                        }
                        return nullptr;
                    } else if(op == any_op::MOVE) {
                        return new(buffer.data()) T(std::move(*reinterpret_cast<T *>(value)));
                    }
                } else {
                    if(op == any_op::CLONE) {
                        return new T(*reinterpret_cast<T *>(value));
                    } else if(op == any_op::DELETE_) {
                        delete static_cast<T*>(value);
                        return nullptr;
                    } else if(op == any_op::MOVE) {
                        if constexpr (std::is_nothrow_move_constructible_v<T>) {
                            return new T(std::move(*reinterpret_cast<T *>(value)));
                        }
                    }
                }
                std::terminate();
            };
            if constexpr (FORMATTABLE) {
                _formatFn = [](void *value) noexcept {
                    return fmt::format("{}", *static_cast<T *>(value));
                };
            }
            _typeName = typeName<std::remove_cvref_t<T>>();

            return *reinterpret_cast<std::decay<T>*>(_ptr);
        }

        void reset() noexcept {
            if(_ptr != nullptr) {
                _opFn(any_op::DELETE_, _buffer, _ptr);
                _ptr = nullptr;
            }
            _size = 0;
            _typeHash = 0;
            _typeName = std::string_view{};
        }

        [[nodiscard]] std::string to_string() const noexcept {
            if(_formatFn == nullptr || _ptr == nullptr) {
                return "Unprintable value";
            }

            return _formatFn(_ptr);
        }

        [[nodiscard]] bool has_value() const noexcept {
            return _ptr != nullptr;
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
        [[nodiscard]] ValueType any_cast() {
            using Up = typename std::remove_pointer_t<std::remove_cvref_t<ValueType>>;
            static_assert((std::is_reference_v<ValueType> || std::is_copy_constructible_v<ValueType>), "Template argument must be a reference or CopyConstructible type");
            static_assert(std::is_constructible_v<ValueType, Up&>, "Template argument must be constructible from an lvalue.");
            if(_ptr != nullptr && _typeHash == typeNameHash<Up>()) {
                return static_cast<ValueType>(*reinterpret_cast<Up*>(_ptr));
            }
            throw bad_any_cast{_typeName, typeName<Up>()};
        }

        template<typename ValueType>
        [[nodiscard]] ValueType any_cast() const {
            using Up = typename std::remove_pointer_t<std::remove_cvref_t<ValueType>>;
            static_assert((std::is_reference_v<ValueType> || std::is_copy_constructible_v<ValueType>), "Template argument must be a reference or CopyConstructible type");
            static_assert(std::is_constructible_v<ValueType, const Up&>, "Template argument must be constructible from a const lvalue.");
            if(_ptr != nullptr && _typeHash == typeNameHash<Up>()) {
                return static_cast<ValueType>(*reinterpret_cast<Up*>(_ptr));
            }
            throw bad_any_cast{_typeName, typeName<Up>()};
        }

        template<typename ValueType>
        [[nodiscard]] ValueType any_cast_ptr() const {
            using Up = typename std::remove_pointer_t<std::remove_cvref_t<ValueType>>;
            static_assert((std::is_pointer_v<ValueType> ), "Template argument must be a pointer type");
            auto comparison = typeNameHash<Up>();
            if(_ptr != nullptr && _typeHash == comparison) {
                return reinterpret_cast<ValueType>(_ptr);
            }
            throw bad_any_cast{_typeName, typeName<Up>()};
        }

    private:
        std::size_t _size{};
        void* _ptr{};
        uint64_t _typeHash{};
        opFnPtr _opFn{};
        formatValueFnPtr _formatFn{};
        std::string_view _typeName{};
        alignas(bufferTypeSize) bufferType _buffer;
    };

    template<typename ValueType>
    [[nodiscard]] ValueType any_cast(const any& a) {
        return a.template any_cast<ValueType>();
    }

    template<typename ValueType>
    [[nodiscard]] ValueType any_cast(any& a) {
        return a.template any_cast<ValueType>();
    }

    template<typename ValueType>
    [[nodiscard]] ValueType any_cast(any const *a) {
        return a->template any_cast_ptr<ValueType>();
    }

    template <typename T, typename... Args>
    [[nodiscard]] any make_any(Args&&... args) noexcept {
        any a;
        a.template emplace<true, T, Args...>(std::forward<Args>(args)...);
        return a;
    }

    template <typename T, typename... Args>
    [[nodiscard]] any make_unformattable_any(Args&&... args) noexcept {
        any a;
        a.template emplace<false, T, Args...>(std::forward<Args>(args)...);
        return a;
    }
}

template <>
struct fmt::formatter<Ichor::any> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::any& change, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "{}", change.to_string());
    }
};
