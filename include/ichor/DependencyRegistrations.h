#pragma once

#include <ichor/Callbacks.h>

namespace Ichor {
    class DependencyManager;

    class [[nodiscard]] EventCompletionHandlerRegistration final {
    public:
        EventCompletionHandlerRegistration(DependencyManager *mgr, CallbackKey key, uint64_t priority) noexcept : _mgr(mgr), _key(key), _priority(priority) {}
        EventCompletionHandlerRegistration() noexcept = default;
        ~EventCompletionHandlerRegistration();

        EventCompletionHandlerRegistration(const EventCompletionHandlerRegistration&) = delete;
        EventCompletionHandlerRegistration(EventCompletionHandlerRegistration&&) noexcept = default;
        EventCompletionHandlerRegistration& operator=(const EventCompletionHandlerRegistration&) = delete;
        EventCompletionHandlerRegistration& operator=(EventCompletionHandlerRegistration&&) noexcept = default;
    private:
        DependencyManager *_mgr{nullptr};
        CallbackKey _key{0, 0};
        uint64_t _priority{0};
    };

    class [[nodiscard]] EventHandlerRegistration final {
    public:
        EventHandlerRegistration(DependencyManager *mgr, CallbackKey key, uint64_t priority) noexcept : _mgr(mgr), _key(key), _priority(priority) {}
        EventHandlerRegistration() noexcept = default;
        ~EventHandlerRegistration();

        EventHandlerRegistration(const EventHandlerRegistration&) = delete;
        EventHandlerRegistration(EventHandlerRegistration&&) noexcept = default;
        EventHandlerRegistration& operator=(const EventHandlerRegistration&) = delete;
        EventHandlerRegistration& operator=(EventHandlerRegistration&&) noexcept = default;
    private:
        DependencyManager *_mgr{nullptr};
        CallbackKey _key{0, 0};
        uint64_t _priority{0};
    };

    class [[nodiscard]] EventInterceptorRegistration final {
    public:
        EventInterceptorRegistration(DependencyManager *mgr, CallbackKey key, uint64_t priority) noexcept : _mgr(mgr), _key(key), _priority(priority) {}
        EventInterceptorRegistration() noexcept = default;
        ~EventInterceptorRegistration();

        EventInterceptorRegistration(const EventInterceptorRegistration&) = delete;
        EventInterceptorRegistration(EventInterceptorRegistration&&) noexcept = default;
        EventInterceptorRegistration& operator=(const EventInterceptorRegistration&) = delete;
        EventInterceptorRegistration& operator=(EventInterceptorRegistration&&) noexcept = default;
    private:
        DependencyManager *_mgr{nullptr};
        CallbackKey _key{0, 0};
        uint64_t _priority{0};
    };



    class [[nodiscard]] DependencyTrackerRegistration final {
    public:
        DependencyTrackerRegistration(DependencyManager *mgr, uint64_t interfaceNameHash, uint64_t priority) noexcept : _mgr(mgr), _interfaceNameHash(interfaceNameHash), _priority(priority) {}
        DependencyTrackerRegistration() noexcept = default;
        ~DependencyTrackerRegistration();

        DependencyTrackerRegistration(const DependencyTrackerRegistration&) = delete;
        DependencyTrackerRegistration(DependencyTrackerRegistration&&) noexcept = default;
        DependencyTrackerRegistration& operator=(const DependencyTrackerRegistration&) = delete;
        DependencyTrackerRegistration& operator=(DependencyTrackerRegistration&&) noexcept = default;
    private:
        DependencyManager *_mgr{nullptr};
        uint64_t _interfaceNameHash{0};
        uint64_t _priority{0};
    };
}