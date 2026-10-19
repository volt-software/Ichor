#pragma once

#include <type_traits>

#include <ichor/ScopedServiceProxy.h>
#include <ichor/stl/CompilerSpecific.h>

// When creating services, the returned pointer is not meant to be stored in a container nor used beyond the scope of that code block.
// So by deleting the copy/move constructors/operators, hopefully force users to store only the service ID.

namespace Ichor::v1 {
    template <typename T>
    struct ServiceProtectedPointer final {
        static_assert(!std::is_pointer_v<T>, "ServiceProtectedPointer<T> already stores a T*, T itself should not be a pointer");

        explicit constexpr ServiceProtectedPointer(T *ptr ICHOR_LIFETIME_BOUND) noexcept : _ptr(ptr) {}
        ServiceProtectedPointer(ServiceProtectedPointer const &) = delete;
        ServiceProtectedPointer(ServiceProtectedPointer &&) = delete;
        ServiceProtectedPointer& operator=(ServiceProtectedPointer const &) = delete;
        ServiceProtectedPointer& operator=(ServiceProtectedPointer &&) = delete;
        ~ServiceProtectedPointer() = default;

        [[nodiscard]] constexpr auto operator->() const noexcept ICHOR_LIFETIME_BOUND {
            return ::Ichor::ScopedServiceProxy<T*>{_ptr, _ptr->getServiceId()};
        }

        [[nodiscard]] constexpr T* get() const noexcept ICHOR_LIFETIME_BOUND {
            return _ptr;
        }

    private:
        T *_ptr;
    };
}
