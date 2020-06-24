#pragma once

#include "ComponentLifecycleManager.h"
namespace Cppelix {
    struct Event {
        Event(uint64_t type) noexcept : _type{type} {}
        virtual ~Event() = default;
        uint64_t _type;
    };

    struct DependencyOnlineEvent : public Event {
        explicit DependencyOnlineEvent(const std::shared_ptr<LifecycleManager> _manager) noexcept :
            Event(type), manager(std::move(_manager)) {}
        ~DependencyOnlineEvent() final = default;

        const std::shared_ptr<LifecycleManager> manager;
        static constexpr uint64_t type = 1;
    };

    struct DependencyOfflineEvent : public Event {
        explicit DependencyOfflineEvent(const std::shared_ptr<LifecycleManager> _manager) noexcept :
            Event(type), manager(std::move(_manager)) {}
        ~DependencyOfflineEvent() final = default;

        const std::shared_ptr<LifecycleManager> manager;
        static constexpr uint64_t type = 2;
    };

    struct DependencyRequestEvent : public Event {
        explicit DependencyRequestEvent(const std::shared_ptr<LifecycleManager> _manager, const std::string_view _requestedType, Dependency _dependency) noexcept :
            Event(type), manager(std::move(_manager)), requestedType(_requestedType), dependency(std::move(_dependency)) {}
        ~DependencyRequestEvent() final = default;

        const std::shared_ptr<LifecycleManager> manager;
        const std::string_view requestedType;
        const Dependency dependency;
        static constexpr uint64_t type = 3;
    };

    struct QuitEvent : public Event {
        QuitEvent(bool _dependenciesStopped = false) noexcept : Event(type), dependenciesStopped(_dependenciesStopped) {}
        ~QuitEvent() final = default;

        bool dependenciesStopped;
        static constexpr uint64_t type = 4;
    };

    struct StopBundleEvent : public Event {
        StopBundleEvent(uint64_t _bundleId, bool _dependenciesStopped = false) noexcept : Event(type), bundleId(_bundleId), dependenciesStopped(_dependenciesStopped) {}
        ~StopBundleEvent() final = default;

        uint64_t bundleId;
        bool dependenciesStopped;
        static constexpr uint64_t type = 5;
    };

    struct StartBundleEvent : public Event {
        StartBundleEvent(uint64_t _bundleId) noexcept : Event(type), bundleId(_bundleId) {}
        ~StartBundleEvent() final = default;

        uint64_t bundleId;
        static constexpr uint64_t type = 6;
    };
}