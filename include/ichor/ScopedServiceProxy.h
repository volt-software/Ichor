#pragma once

#include <ichor/ConstevalHash.h>
#include <ichor/CoreTypes.h>
#include <ichor/Defines.h>
#include <ichor/stl/NeverAlwaysNull.h>
#include <type_traits>
#include <cstddef>
#include <utility>

#if defined(ICHOR_ENABLE_INTERNAL_DEBUGGING) || defined(ICHOR_USE_HARDENING)
#include <ichor/interfaces/IFrameworkLogger.h>
#include <ichor/ConstevalHash.h>
#endif

namespace Ichor {
    template<typename Service>
    class ScopedServiceProxy final {
    public:
        using Element = std::remove_pointer_t<Service>;
        using Pointer = Element*;

        ScopedServiceProxy() noexcept = default;
        explicit ScopedServiceProxy(Ichor::v1::NeverNull<Pointer> svc, ServiceIdType id) noexcept : _service(svc), _serviceId(id) {}

        ScopedServiceProxy(ScopedServiceProxy const &) = default;
        ScopedServiceProxy &operator=(ScopedServiceProxy const &) = default;

        ScopedServiceProxy(ScopedServiceProxy &&) noexcept = default;
        ScopedServiceProxy &operator=(ScopedServiceProxy &&) noexcept = default;
        ScopedServiceProxy &operator=(std::nullptr_t) noexcept {
            reset();
            return *this;
        }

        Pointer operator->() const noexcept {
            if constexpr(DO_INTERNAL_DEBUG || DO_HARDENING) {
                if(_service == nullptr) [[unlikely]] {
                    ICHOR_EMERGENCY_NO_LOGGER_LOG2("attempt to use a nullptr service of type {}, perhaps this is a use-after-coroutine?", typeName<Service>());
                    std::terminate();
                }
            }
            return _service;
        }

        Pointer operator*() const noexcept {
            if constexpr(DO_INTERNAL_DEBUG || DO_HARDENING) {
                if(_service == nullptr) [[unlikely]] {
                    ICHOR_EMERGENCY_NO_LOGGER_LOG2("attempt to use a nullptr service of type {}, perhaps this is a use-after-coroutine?", typeName<Service>());
                    std::terminate();
                }
            }
            return _service;
        }

        explicit operator bool() const noexcept {
            return _service != nullptr;
        }

        [[nodiscard]] bool has_value() const noexcept {
            return _service != nullptr;
        }

        [[nodiscard]] ServiceIdType serviceId() const noexcept {
            return _serviceId;
        }

        void reset() noexcept {
            _service = nullptr;
            _serviceId = ServiceIdType{0};
        }

        friend bool operator==(ScopedServiceProxy const &proxyA, ScopedServiceProxy const &proxyB) noexcept {
            return proxyA._serviceId == proxyB._serviceId;
        }

        friend bool operator!=(ScopedServiceProxy const &proxyA, ScopedServiceProxy const &proxyB) noexcept {
            return proxyA._serviceId != proxyB._serviceId;
        }

        friend bool operator==(ScopedServiceProxy const &proxy, std::nullptr_t) noexcept {
            return proxy._service == nullptr;
        }

        friend bool operator==(std::nullptr_t, ScopedServiceProxy const &proxy) noexcept {
            return proxy._service == nullptr;
        }

        friend bool operator!=(ScopedServiceProxy const &proxy, std::nullptr_t) noexcept {
            return proxy._service != nullptr;
        }

        friend bool operator!=(std::nullptr_t, ScopedServiceProxy const &proxy) noexcept {
            return proxy._service != nullptr;
        }
#ifndef ICHOR_ENABLE_INTERNAL_DEBUGGING
    private:
#endif
        Pointer _service{};
        ServiceIdType _serviceId{};
    };

} // namespace Ichor::Detail
