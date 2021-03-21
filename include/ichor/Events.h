#pragma once

#include <memory>
#include <ichor/Dependency.h>
#include <ichor/Callbacks.h>

namespace Ichor {
    class ILifecycleManager;
    class DependencyManager;

    constexpr uint64_t INTERNAL_EVENT_PRIORITY = 1000;

    struct Event {
        Event(uint64_t _type, std::string_view _name, uint64_t _id, uint64_t _originatingService, uint64_t _priority) noexcept : type{_type}, name{_name}, id{_id}, originatingService{_originatingService}, priority{_priority} {}
        virtual ~Event() = default;
        const uint64_t type;
        const std::string_view name;
        const uint64_t id;
        const uint64_t originatingService;
        const uint64_t priority;
    };

    struct DependencyOnlineEvent final : public Event {
        explicit DependencyOnlineEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority) noexcept :
            Event(TYPE, NAME, _id, _originatingService, _priority) {}
        ~DependencyOnlineEvent() final = default;

        static constexpr uint64_t TYPE = typeNameHash<DependencyOnlineEvent>();
        static constexpr std::string_view NAME = typeName<DependencyOnlineEvent>();
    };

    struct DependencyOfflineEvent final : public Event {
        explicit DependencyOfflineEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority) noexcept :
            Event(TYPE, NAME, _id, _originatingService, _priority) {}
        ~DependencyOfflineEvent() final = default;

        static constexpr uint64_t TYPE = typeNameHash<DependencyOfflineEvent>();
        static constexpr std::string_view NAME = typeName<DependencyOfflineEvent>();
    };

    struct DependencyRequestEvent final : public Event {
        explicit DependencyRequestEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, Dependency _dependency, std::optional<IchorProperties const *> _properties) noexcept :
                Event(TYPE, NAME, _id, _originatingService, _priority), dependency(_dependency), properties{_properties} {}
        ~DependencyRequestEvent() final = default;

        const Dependency dependency;
        const std::optional<IchorProperties const *> properties;
        static constexpr uint64_t TYPE = typeNameHash<DependencyRequestEvent>();
        static constexpr std::string_view NAME = typeName<DependencyRequestEvent>();
    };

    struct DependencyUndoRequestEvent final : public Event {
        explicit DependencyUndoRequestEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, Dependency _dependency, IchorProperties const * _properties) noexcept :
                Event(TYPE, NAME, _id, _originatingService, _priority), dependency(_dependency), properties{_properties} {}
        ~DependencyUndoRequestEvent() final = default;

        const Dependency dependency;
        const IchorProperties * properties;
        static constexpr uint64_t TYPE = typeNameHash<DependencyUndoRequestEvent>();
        static constexpr std::string_view NAME = typeName<DependencyUndoRequestEvent>();
    };

    struct QuitEvent final : public Event {
        QuitEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, bool _dependenciesStopped = false) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority), dependenciesStopped(_dependenciesStopped) {}
        ~QuitEvent() final = default;

        const bool dependenciesStopped;
        static constexpr uint64_t TYPE = typeNameHash<QuitEvent>();
        static constexpr std::string_view NAME = typeName<QuitEvent>();
    };

    struct StopServiceEvent final : public Event {
        StopServiceEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, uint64_t _serviceId, bool _dependenciesStopped = false) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority), serviceId(_serviceId), dependenciesStopped(_dependenciesStopped) {}
        ~StopServiceEvent() final = default;

        const uint64_t serviceId;
        const bool dependenciesStopped;
        static constexpr uint64_t TYPE = typeNameHash<StopServiceEvent>();
        static constexpr std::string_view NAME = typeName<StopServiceEvent>();
    };

    struct StartServiceEvent final : public Event {
        StartServiceEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, uint64_t _serviceId) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority), serviceId(_serviceId) {}
        ~StartServiceEvent() final = default;

        const uint64_t serviceId;
        static constexpr uint64_t TYPE = typeNameHash<StartServiceEvent>();
        static constexpr std::string_view NAME = typeName<StartServiceEvent>();
    };

    struct RemoveServiceEvent final : public Event {
        RemoveServiceEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, uint64_t _serviceId, bool _dependenciesStopped = false) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority), serviceId(_serviceId), dependenciesStopped(_dependenciesStopped) {}
        ~RemoveServiceEvent() final = default;

        const uint64_t serviceId;
        const bool dependenciesStopped;
        static constexpr uint64_t TYPE = typeNameHash<RemoveServiceEvent>();
        static constexpr std::string_view NAME = typeName<RemoveServiceEvent>();
    };

    struct DoWorkEvent final : public Event {
        DoWorkEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority) {}
        ~DoWorkEvent() final = default;

        static constexpr uint64_t TYPE = typeNameHash<DoWorkEvent>();
        static constexpr std::string_view NAME = typeName<DoWorkEvent>();
    };

    struct RemoveCompletionCallbacksEvent final : public Event {
        RemoveCompletionCallbacksEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, CallbackKey _key) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority), key(_key) {}
        ~RemoveCompletionCallbacksEvent() final = default;

        const CallbackKey key;
        static constexpr uint64_t TYPE = typeNameHash<RemoveCompletionCallbacksEvent>();
        static constexpr std::string_view NAME = typeName<RemoveCompletionCallbacksEvent>();
    };

    struct RemoveEventHandlerEvent final : public Event {
        RemoveEventHandlerEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, CallbackKey _key) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority), key(_key) {}
        ~RemoveEventHandlerEvent() final = default;

        const CallbackKey key;
        static constexpr uint64_t TYPE = typeNameHash<RemoveEventHandlerEvent>();
        static constexpr std::string_view NAME = typeName<RemoveEventHandlerEvent>();
    };

    struct RemoveEventInterceptorEvent final : public Event {
        RemoveEventInterceptorEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, CallbackKey _key) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority), key(_key) {}
        ~RemoveEventInterceptorEvent() final = default;

        const CallbackKey key;
        static constexpr uint64_t TYPE = typeNameHash<RemoveEventInterceptorEvent>();
        static constexpr std::string_view NAME = typeName<RemoveEventInterceptorEvent>();
    };

    struct RemoveTrackerEvent final : public Event {
        RemoveTrackerEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, uint64_t _interfaceNameHash) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority), interfaceNameHash(_interfaceNameHash) {}
        ~RemoveTrackerEvent() final = default;

        const uint64_t interfaceNameHash;
        static constexpr uint64_t TYPE = typeNameHash<RemoveTrackerEvent>();
        static constexpr std::string_view NAME = typeName<RemoveTrackerEvent>();
    };

    template <typename GeneratorT>
    struct ContinuableEvent final : public Event {
        ContinuableEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, GeneratorT _generator) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority), generator(std::move(_generator)) {}
        ~ContinuableEvent() final = default;

        GeneratorT generator;
        static constexpr uint64_t TYPE = typeNameHash<ContinuableEvent<GeneratorT>>();
        static constexpr std::string_view NAME = typeName<ContinuableEvent<GeneratorT>>();
    };

    struct UnrecoverableErrorEvent final : public Event {
        UnrecoverableErrorEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, uint64_t _errorType, std::string _error) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority), errorType(_errorType), error(std::move(_error)) {}
        ~UnrecoverableErrorEvent() final = default;

        uint64_t errorType;
        std::string error;
        static constexpr uint64_t TYPE = typeNameHash<UnrecoverableErrorEvent>();
        static constexpr std::string_view NAME = typeName<UnrecoverableErrorEvent>();
    };

    struct RecoverableErrorEvent final : public Event {
        RecoverableErrorEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, uint64_t _errorType, std::string _error) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority), errorType(_errorType), error(std::move(_error)) {}
        ~RecoverableErrorEvent() final = default;

        uint64_t errorType;
        std::string error;
        static constexpr uint64_t TYPE = typeNameHash<RecoverableErrorEvent>();
        static constexpr std::string_view NAME = typeName<RecoverableErrorEvent>();
    };

    struct RunFunctionEvent final : public Event {
        RunFunctionEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, std::function<void(DependencyManager*)> _fun) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority), fun(std::move(_fun)) {}
        ~RunFunctionEvent() final = default;

        std::function<void(DependencyManager*)> fun;
        static constexpr uint64_t TYPE = typeNameHash<RunFunctionEvent>();
        static constexpr std::string_view NAME = typeName<RunFunctionEvent>();
    };
}