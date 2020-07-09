#pragma once

#include "Dependency.h"
#include "Callback.h"

namespace Cppelix {
    class LifecycleManager;

    struct Event {
        Event(uint64_t _type, uint64_t _id, uint64_t _originatingService) noexcept : type{_type}, id{_id}, originatingService{_originatingService} {}
        virtual ~Event() = default;
        const uint64_t type;
        const uint64_t id;
        const uint64_t originatingService;
    };

    struct DependencyOnlineEvent final : public Event {
        explicit DependencyOnlineEvent(uint64_t _id, uint64_t _originatingService, const std::shared_ptr<LifecycleManager> _manager) noexcept :
            Event(TYPE, _id, _originatingService), manager(std::move(_manager)) {}
        ~DependencyOnlineEvent() final = default;

        const std::shared_ptr<LifecycleManager> manager;
        static constexpr uint64_t TYPE = typeNameHash<DependencyOnlineEvent>();
    };

    struct DependencyOfflineEvent final : public Event {
        explicit DependencyOfflineEvent(uint64_t _id, uint64_t _originatingService, const std::shared_ptr<LifecycleManager> _manager) noexcept :
            Event(TYPE, _id, _originatingService), manager(std::move(_manager)) {}
        ~DependencyOfflineEvent() final = default;

        const std::shared_ptr<LifecycleManager> manager;
        static constexpr uint64_t TYPE = typeNameHash<DependencyOfflineEvent>();
    };

    struct DependencyRequestEvent final : public Event {
        explicit DependencyRequestEvent(uint64_t _id, uint64_t _originatingService, std::shared_ptr<LifecycleManager> _manager, Dependency _dependency, CppelixProperties const * _properties) noexcept :
                Event(TYPE, _id, _originatingService), manager(std::move(_manager)), dependency(std::move(_dependency)), properties{_properties} {}
        ~DependencyRequestEvent() final = default;

        const std::shared_ptr<LifecycleManager> manager;
        const Dependency dependency;
        const CppelixProperties * properties;
        static constexpr uint64_t TYPE = typeNameHash<DependencyRequestEvent>();
    };

    struct DependencyUndoRequestEvent final : public Event {
        explicit DependencyUndoRequestEvent(uint64_t _id, uint64_t _originatingService, std::shared_ptr<LifecycleManager> _manager, Dependency _dependency, CppelixProperties const * _properties) noexcept :
                Event(TYPE, _id, _originatingService), manager(std::move(_manager)), dependency(std::move(_dependency)), properties{_properties} {}
        ~DependencyUndoRequestEvent() final = default;

        const std::shared_ptr<LifecycleManager> manager;
        const Dependency dependency;
        const CppelixProperties * properties;
        static constexpr uint64_t TYPE = typeNameHash<DependencyUndoRequestEvent>();
    };

    struct QuitEvent final : public Event {
        QuitEvent(uint64_t _id, uint64_t _originatingService, bool _dependenciesStopped = false) noexcept : Event(TYPE, _id, _originatingService), dependenciesStopped(_dependenciesStopped) {}
        ~QuitEvent() final = default;

        const bool dependenciesStopped;
        static constexpr uint64_t TYPE = typeNameHash<QuitEvent>();
    };

    struct StopServiceEvent final : public Event {
        StopServiceEvent(uint64_t _id, uint64_t _originatingService, uint64_t _serviceId, bool _dependenciesStopped = false) noexcept : Event(TYPE, _id, _originatingService), serviceId(_serviceId), dependenciesStopped(_dependenciesStopped) {}
        ~StopServiceEvent() final = default;

        const uint64_t serviceId;
        const bool dependenciesStopped;
        static constexpr uint64_t TYPE = typeNameHash<StopServiceEvent>();
    };

    struct StartServiceEvent final : public Event {
        StartServiceEvent(uint64_t _id, uint64_t _originatingService, uint64_t _serviceId) noexcept : Event(TYPE, _id, _originatingService), serviceId(_serviceId) {}
        ~StartServiceEvent() final = default;

        const uint64_t serviceId;
        static constexpr uint64_t TYPE = typeNameHash<StartServiceEvent>();
    };

    struct RemoveServiceEvent final : public Event {
        RemoveServiceEvent(uint64_t _id, uint64_t _originatingService, uint64_t _serviceId, bool _dependenciesStopped = false) noexcept : Event(TYPE, _id, _originatingService), serviceId(_serviceId), dependenciesStopped(_dependenciesStopped) {}
        ~RemoveServiceEvent() final = default;

        const uint64_t serviceId;
        const bool dependenciesStopped;
        static constexpr uint64_t TYPE = typeNameHash<RemoveServiceEvent>();
    };

    struct DoWorkEvent final : public Event {
        DoWorkEvent(uint64_t _id, uint64_t _originatingService) noexcept : Event(TYPE, _id, _originatingService) {}
        ~DoWorkEvent() final = default;

        static constexpr uint64_t TYPE = typeNameHash<DoWorkEvent>();
    };

    struct RemoveCallbacksEvent final : public Event {
        RemoveCallbacksEvent(uint64_t _id, uint64_t _originatingService, CallbackKey _key) noexcept : Event(TYPE, _id, _originatingService), key(_key) {}
        ~RemoveCallbacksEvent() final = default;

        const CallbackKey key;
        static constexpr uint64_t TYPE = typeNameHash<RemoveCallbacksEvent>();
    };

    struct RemoveTrackerEvent final : public Event {
        RemoveTrackerEvent(uint64_t _id, uint64_t _originatingService, uint64_t _interfaceNameHash) noexcept : Event(TYPE, _id, _originatingService), interfaceNameHash(_interfaceNameHash) {}
        ~RemoveTrackerEvent() final = default;

        const uint64_t interfaceNameHash;
        static constexpr uint64_t TYPE = typeNameHash<RemoveTrackerEvent>();
    };
}