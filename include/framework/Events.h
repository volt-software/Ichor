#pragma once

#include "ServiceLifecycleManager.h"
namespace Cppelix {
    struct Event {
        Event(uint64_t _type, uint64_t _id, uint64_t _originatingService) noexcept : type{_type}, id{_id}, originatingService{_originatingService} {}
        virtual ~Event() = default;
        uint64_t type;
        uint64_t id;
        uint64_t originatingService;
    };

    struct DependencyOnlineEvent : public Event {
        explicit DependencyOnlineEvent(uint64_t _id, uint64_t _originatingService, const std::shared_ptr<LifecycleManager> _manager) noexcept :
            Event(TYPE, _id, _originatingService), manager(std::move(_manager)) {}
        ~DependencyOnlineEvent() final = default;

        const std::shared_ptr<LifecycleManager> manager;
        static constexpr uint64_t TYPE = 1;
    };

    struct DependencyOfflineEvent : public Event {
        explicit DependencyOfflineEvent(uint64_t _id, uint64_t _originatingService, const std::shared_ptr<LifecycleManager> _manager) noexcept :
            Event(TYPE, _id, _originatingService), manager(std::move(_manager)) {}
        ~DependencyOfflineEvent() final = default;

        const std::shared_ptr<LifecycleManager> manager;
        static constexpr uint64_t TYPE = 2;
    };

    struct DependencyRequestEvent : public Event {
        explicit DependencyRequestEvent(uint64_t _id, uint64_t _originatingService, const std::shared_ptr<LifecycleManager> _manager, uint64_t _requestedType, Dependency _dependency) noexcept :
            Event(TYPE, _id, _originatingService), manager(std::move(_manager)), requestedType(_requestedType), dependency(std::move(_dependency)) {}
        ~DependencyRequestEvent() final = default;

        const std::shared_ptr<LifecycleManager> manager;
        const uint64_t requestedType;
        const Dependency dependency;
        static constexpr uint64_t TYPE = 3;
    };

    struct QuitEvent : public Event {
        QuitEvent(uint64_t _id, uint64_t _originatingService, bool _dependenciesStopped = false) noexcept : Event(TYPE, _id, _originatingService), dependenciesStopped(_dependenciesStopped) {}
        ~QuitEvent() final = default;

        bool dependenciesStopped;
        static constexpr uint64_t TYPE = 4;
    };

    struct StopServiceEvent : public Event {
        StopServiceEvent(uint64_t _id, uint64_t _originatingService, uint64_t _serviceId, bool _dependenciesStopped = false) noexcept : Event(TYPE, _id, _originatingService), serviceId(_serviceId), dependenciesStopped(_dependenciesStopped) {}
        ~StopServiceEvent() final = default;

        uint64_t serviceId;
        bool dependenciesStopped;
        static constexpr uint64_t TYPE = 5;
    };

    struct StartServiceEvent : public Event {
        StartServiceEvent(uint64_t _id, uint64_t _originatingService, uint64_t _serviceId) noexcept : Event(TYPE, _id, _originatingService), serviceId(_serviceId) {}
        ~StartServiceEvent() final = default;

        uint64_t serviceId;
        static constexpr uint64_t TYPE = 6;
    };
}