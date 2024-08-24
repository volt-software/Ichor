#pragma once

#include <type_traits>

// When creating services, the returned pointer is not meant to be stored in a container nor used beyond the scope of that code block.
// So by deleting the copy/move constructors/operators, hopefully force users to store only the service ID.

namespace Ichor {
    template <typename T>
    struct ServiceProtectedPointer final {
        explicit ServiceProtectedPointer(T *ptr) noexcept : _ptr(ptr) {}
        ServiceProtectedPointer(ServiceProtectedPointer const &) = delete;
        ServiceProtectedPointer(ServiceProtectedPointer &&) = delete;
        ServiceProtectedPointer& operator=(ServiceProtectedPointer const &) = delete;
        ServiceProtectedPointer& operator=(ServiceProtectedPointer &&) = delete;

        [[nodiscard]] std::remove_pointer_t<T>* operator->() const noexcept {
            return _ptr;
        }

        [[nodiscard]] std::remove_pointer_t<T>* get() const noexcept {
            return _ptr;
        }

    private:
        std::remove_pointer_t<T> *_ptr;
    };
}
