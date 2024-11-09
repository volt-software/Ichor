#pragma once

#include <type_traits> // for false_type, is_convertible etc
#include <utility> // for forward, move
#include <system_error> // for hash
#include <cstddef> // for ptrdiff_t, etc
#include <cassert>

// Copied from Microsoft GSL: https://github.com/microsoft/GSL/blob/main/include/gsl/pointers
namespace Ichor {

    namespace Detail {
        template <typename T, typename = void>
        struct is_comparable_to_nullptr : std::false_type {
        };

        template <typename T>
        struct is_comparable_to_nullptr<T, std::enable_if_t<std::is_convertible<decltype(std::declval<T>() != nullptr), bool>::value>> : std::true_type {
        };

        // Resolves to the more efficient of `const T` or `const T&`, in the context of returning a const-qualified value
        // of type T.
        //
        // Copied from cppfront's implementation of the CppCoreGuidelines F.16 (https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rf-in)
        template<typename T>
        using value_or_reference_return_t = std::conditional_t<sizeof(T) < 2*sizeof(void*) && std::is_trivially_copy_constructible<T>::value, const T, const T&>;
    }

    template <typename T>
    class NeverNull {
    public:
        static_assert(Detail::is_comparable_to_nullptr<T>::value, "T cannot be compared to nullptr.");
        static_assert(std::is_pointer_v<T>, "T has to be a pointer.");

        template <typename U, typename = std::enable_if_t<std::is_convertible<U, T>::value>>
        constexpr NeverNull(U&& u) : ptr_(std::forward<U>(u))
        {
            assert(ptr_ != nullptr);
        }

        template <typename = std::enable_if_t<!std::is_same<std::nullptr_t, T>::value>>
        constexpr NeverNull(T u) : ptr_(std::move(u))
        {
            assert(ptr_ != nullptr);
        }

        template <typename U, typename = std::enable_if_t<std::is_convertible<U, T>::value>>
        constexpr NeverNull(const NeverNull<U>& other) : NeverNull(other.get())
        {}

        NeverNull(const NeverNull& other) = default;
        NeverNull& operator=(const NeverNull& other) = default;
        constexpr Detail::value_or_reference_return_t<T> get() const
        {
            return ptr_;
        }

        constexpr operator T() const { return get(); }
        constexpr decltype(auto) operator->() const { return get(); }
        constexpr decltype(auto) operator*() const { return *get(); }

        // prevents compilation when someone attempts to assign a null pointer constant
        NeverNull(std::nullptr_t) = delete;
        NeverNull& operator=(std::nullptr_t) = delete;

        // unwanted operators...pointers only point to single objects!
        NeverNull& operator++() = delete;
        NeverNull& operator--() = delete;
        NeverNull operator++(int) = delete;
        NeverNull operator--(int) = delete;
        NeverNull& operator+=(std::ptrdiff_t) = delete;
        NeverNull& operator-=(std::ptrdiff_t) = delete;
        void operator[](std::ptrdiff_t) const = delete;
    private:
        T ptr_;
    };

    template <class T, class U>
    auto operator==(const NeverNull<T>& lhs, const NeverNull<U>& rhs) noexcept(noexcept(lhs.get() == rhs.get()))
    -> decltype(lhs.get() == rhs.get()) {
        return lhs.get() == rhs.get();
    }

    template <class T, class U>
    auto operator!=(const NeverNull<T>& lhs, const NeverNull<U>& rhs) noexcept(noexcept(lhs.get() != rhs.get()))
    -> decltype(lhs.get() != rhs.get()) {
        return lhs.get() != rhs.get();
    }

    template <class T, class U>
    auto operator<(const NeverNull<T>& lhs, const NeverNull<U>& rhs) noexcept(noexcept(lhs.get() < rhs.get()))
    -> decltype(lhs.get() < rhs.get()) {
        return lhs.get() < rhs.get();
    }

    template <class T, class U>
    auto operator<=(const NeverNull<T>& lhs, const NeverNull<U>& rhs) noexcept(noexcept(lhs.get() <= rhs.get()))
    -> decltype(lhs.get() <= rhs.get()) {
        return lhs.get() <= rhs.get();
    }

    template <class T, class U>
    auto operator>(const NeverNull<T>& lhs, const NeverNull<U>& rhs) noexcept(noexcept(lhs.get() > rhs.get()))
    -> decltype(lhs.get() > rhs.get()) {
        return lhs.get() > rhs.get();
    }

    template <class T, class U>
    auto operator>=(const NeverNull<T>& lhs, const NeverNull<U>& rhs) noexcept(noexcept(lhs.get() >= rhs.get()))
    -> decltype(lhs.get() >= rhs.get()) {
        return lhs.get() >= rhs.get();
    }

    // more unwanted operators
    template <class T, class U>
    std::ptrdiff_t operator-(const NeverNull<T>&, const NeverNull<U>&) = delete;
    template <class T>
    NeverNull<T> operator-(const NeverNull<T>&, std::ptrdiff_t) = delete;
    template <class T>
    NeverNull<T> operator+(const NeverNull<T>&, std::ptrdiff_t) = delete;
    template <class T>
    NeverNull<T> operator+(std::ptrdiff_t, const NeverNull<T>&) = delete;

    template <typename T>
    class AlwaysNull {
    public:
        static_assert(Detail::is_comparable_to_nullptr<T>::value, "T cannot be compared to nullptr.");
        constexpr AlwaysNull() = default;
        AlwaysNull(const AlwaysNull& other) = default;
        AlwaysNull& operator=(const AlwaysNull& other) = default;

        constexpr operator T() const = delete;
        constexpr decltype(auto) operator->() const = delete;
        constexpr decltype(auto) operator*() const = delete;

        // prevents compilation when someone attempts to assign a null pointer constant
        AlwaysNull(std::nullptr_t) = delete;
        AlwaysNull& operator=(std::nullptr_t) = delete;

        // unwanted operators...pointers only point to single objects!
        AlwaysNull& operator++() = delete;
        AlwaysNull& operator--() = delete;
        AlwaysNull operator++(int) = delete;
        AlwaysNull operator--(int) = delete;
        AlwaysNull& operator+=(std::ptrdiff_t) = delete;
        AlwaysNull& operator-=(std::ptrdiff_t) = delete;
        void operator[](std::ptrdiff_t) const = delete;
    };

    // more unwanted operators
    template <class T, class U>
    std::ptrdiff_t operator-(const AlwaysNull<T>&, const AlwaysNull<U>&) = delete;
    template <class T>
    AlwaysNull<T> operator-(const AlwaysNull<T>&, std::ptrdiff_t) = delete;
    template <class T>
    AlwaysNull<T> operator+(const AlwaysNull<T>&, std::ptrdiff_t) = delete;
    template <class T>
    AlwaysNull<T> operator+(std::ptrdiff_t, const AlwaysNull<T>&) = delete;
}

namespace std
{
    template <class T>
    struct hash<Ichor::NeverNull<T>>
    {
        std::size_t operator()(const Ichor::NeverNull<T>& value) const { return hash<T>{}(value.get()); }
    };

} // namespace std
