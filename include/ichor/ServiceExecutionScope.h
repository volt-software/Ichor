#pragma once

#include <ichor/Defines.h>
#include <ichor/stl/NeverAlwaysNull.h>
#include <type_traits>
#include <cstddef>
#include <utility>

#ifdef ICHOR_HAVE_STD_STACKTRACE
#define ICHOR_DEBUG_PROXIES_CALLSTACK 1
#else
#define ICHOR_DEBUG_PROXIES_CALLSTACK 0
#endif

#if ICHOR_DEBUG_PROXIES_CALLSTACK
#include <stacktrace>
#endif

namespace Ichor::Detail {

    struct ServiceExecutionScopeContents final {
        ServiceIdType id;
#if ICHOR_DEBUG_PROXIES_CALLSTACK
        std::stacktrace trace;

        ServiceExecutionScopeContents(ServiceIdType _id) : id(_id), trace(std::stacktrace::current()) {}
#else
        ServiceExecutionScopeContents(ServiceIdType _id) : id(_id) {}
#endif

        friend bool operator==(ServiceExecutionScopeContents const &content, ServiceIdType _id) noexcept {
            return content.id == _id;
        }
        friend bool operator==(ServiceIdType _id, ServiceExecutionScopeContents const &content) noexcept {
            return content.id == _id;
        }
    };

    class ServiceExecutionScope final {
    public:
        explicit ServiceExecutionScope(ServiceIdType id) noexcept {
            // fmt::println("ServiceExecutionScope pushing {}", id);
            current().push_back(id);
        }
        ~ServiceExecutionScope() noexcept {
            auto &stack = current();
            if(!stack.empty()) {
            // fmt::println("ServiceExecutionScope popping {}", stack.back().id);
                stack.pop_back();
            }
        }

        ServiceExecutionScope(ServiceExecutionScope const &) = delete;
        ServiceExecutionScope &operator=(ServiceExecutionScope const &) = delete;

        ServiceExecutionScope(ServiceExecutionScope &&other) noexcept = delete;
        ServiceExecutionScope &operator=(ServiceExecutionScope &&other) noexcept = delete;

        [[nodiscard]] static ServiceIdType currentServiceId() noexcept {
            auto &stack = current();
            return stack.empty() ? 0 : stack.back().id;
        }

        [[nodiscard]] std::vector<ServiceExecutionScopeContents> &getServiceIdStack() noexcept {
            return current();
        }

    private:
        [[nodiscard]] static std::vector<ServiceExecutionScopeContents> &current() noexcept {
            ICHOR_CONSTINIT_VECTOR thread_local std::vector<ServiceExecutionScopeContents> stack;
            return stack;
        }
    };

    template<typename Service>
    class ScopedServiceProxy final {
    public:
        using Element = std::remove_pointer_t<Service>;
        using Pointer = Element*;

        struct CallScopeProxy {
            Pointer service;
            mutable ServiceExecutionScope scope;
            Pointer operator->() const noexcept {
                return service;
             }
        };

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

        CallScopeProxy operator->() const noexcept {
            return CallScopeProxy{_service, Detail::ServiceExecutionScope{_serviceId}};
        }

        CallScopeProxy operator*() const noexcept {
            return CallScopeProxy{_service, Detail::ServiceExecutionScope{_serviceId}};
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
            _serviceId = 0;
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

namespace Ichor {
    template<typename Service>
    using ScopedServiceProxy = Detail::ScopedServiceProxy<Service>;
}

