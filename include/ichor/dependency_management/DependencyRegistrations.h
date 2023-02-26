#pragma once

#include <ichor/Callbacks.h>

namespace Ichor {
    class [[nodiscard]] EventCompletionHandlerRegistration final {
    public:
        EventCompletionHandlerRegistration(CallbackKey key, uint64_t priority) noexcept : _key(key), _priority(priority) {}
        EventCompletionHandlerRegistration() noexcept = default;
        ~EventCompletionHandlerRegistration();

        EventCompletionHandlerRegistration(const EventCompletionHandlerRegistration&) = delete;
        EventCompletionHandlerRegistration(EventCompletionHandlerRegistration&& o) noexcept : _key(o._key), _priority(o._priority) {
            o._key.type = 0;
        }
        EventCompletionHandlerRegistration& operator=(const EventCompletionHandlerRegistration&) = delete;
        EventCompletionHandlerRegistration& operator=(EventCompletionHandlerRegistration&& o) noexcept {
            _key = o._key;
            _priority = o._priority;
            o._key.type = 0;
            return *this;
        }

        void reset();
    private:
        CallbackKey _key{0, 0};
        uint64_t _priority{0};
    };

    class [[nodiscard]] EventHandlerRegistration final {
    public:
        EventHandlerRegistration(CallbackKey key, uint64_t priority) noexcept : _key(key), _priority(priority) {}
        EventHandlerRegistration() noexcept = default;
        ~EventHandlerRegistration();

        EventHandlerRegistration(const EventHandlerRegistration&) = delete;
        EventHandlerRegistration(EventHandlerRegistration&& o) noexcept : _key(o._key), _priority(o._priority) {
            o._key.type = 0;
        }
        EventHandlerRegistration& operator=(const EventHandlerRegistration&) = delete;
        EventHandlerRegistration& operator=(EventHandlerRegistration&& o) noexcept {
            _key = o._key;
            _priority = o._priority;
            o._key.type = 0;
            return *this;
        }

        void reset();
    private:
        CallbackKey _key{0, 0};
        uint64_t _priority{0};
    };

    class [[nodiscard]] EventInterceptorRegistration final {
    public:
        EventInterceptorRegistration(CallbackKey key, uint64_t priority) noexcept : _key(key), _priority(priority) {}
        EventInterceptorRegistration() noexcept = default;
        ~EventInterceptorRegistration();

        EventInterceptorRegistration(const EventInterceptorRegistration&) = delete;
        EventInterceptorRegistration(EventInterceptorRegistration&& o) noexcept : _key(o._key), _priority(o._priority) {
            o._key.type = 0;
        }
        EventInterceptorRegistration& operator=(const EventInterceptorRegistration&) = delete;
        EventInterceptorRegistration& operator=(EventInterceptorRegistration&& o) noexcept {
            _key = o._key;
            _priority = o._priority;
            o._key.type = 0;
            return *this;
        }

        void reset();
    private:
        CallbackKey _key{0, 0};
        uint64_t _priority{0};
    };



    class [[nodiscard]] DependencyTrackerRegistration final {
    public:
        DependencyTrackerRegistration(uint64_t serviceId, uint64_t interfaceNameHash, uint64_t priority) noexcept : _serviceId(serviceId), _interfaceNameHash(interfaceNameHash), _priority(priority) {}
        DependencyTrackerRegistration() noexcept = default;
        ~DependencyTrackerRegistration();

        DependencyTrackerRegistration(const DependencyTrackerRegistration&) = delete;
        DependencyTrackerRegistration(DependencyTrackerRegistration&& o) noexcept : _serviceId(o._serviceId), _interfaceNameHash(o._interfaceNameHash), _priority(o._priority) {
            o._interfaceNameHash = 0;
        }
        DependencyTrackerRegistration& operator=(const DependencyTrackerRegistration&) = delete;
        DependencyTrackerRegistration& operator=(DependencyTrackerRegistration&& o) noexcept {
            _serviceId = o._serviceId;
            _interfaceNameHash = o._interfaceNameHash;
            _priority = o._priority;
            o._interfaceNameHash = 0;
            return *this;
        }

        void reset();
    private:
        uint64_t _serviceId{0};
        uint64_t _interfaceNameHash{0};
        uint64_t _priority{0};
    };
}
