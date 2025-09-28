#pragma once

#include <type_traits>

#include <ichor/ServiceExecutionScope.h>

// When creating services, the returned pointer is not meant to be stored in a container nor used beyond the scope of that code block.
// So by deleting the copy/move constructors/operators, hopefully force users to store only the service ID.

namespace Ichor::v1 {
    template <typename T>
    struct ServiceProtectedPointer final {
        explicit constexpr ServiceProtectedPointer(T *ptr ICHOR_LIFETIME_BOUND) noexcept : _ptr(ptr) {}
        ServiceProtectedPointer(ServiceProtectedPointer const &) = delete;
        ServiceProtectedPointer(ServiceProtectedPointer &&) = delete;
        ServiceProtectedPointer& operator=(ServiceProtectedPointer const &) = delete;
        ServiceProtectedPointer& operator=(ServiceProtectedPointer &&) = delete;

        [[nodiscard]] constexpr auto operator->() const noexcept ICHOR_LIFETIME_BOUND {
            using ServiceType = std::remove_pointer_t<T>;
            return ::Ichor::ScopedServiceProxy<ServiceType*>{_ptr, _ptr->getServiceId()};
        }

        [[nodiscard]] constexpr std::remove_pointer_t<T>* get() const noexcept ICHOR_LIFETIME_BOUND {
            return _ptr;
        }

    private:
        std::remove_pointer_t<T> *_ptr;
    };
}
