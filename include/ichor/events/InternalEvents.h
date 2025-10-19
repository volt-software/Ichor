#pragma once

#include <ichor/events/Event.h>
#include <ichor/ConstevalHash.h>
#include <ichor/dependency_management/Dependency.h>
#include <ichor/Callbacks.h>
#include <ichor/stl/NeverAlwaysNull.h>
#include <ichor/stl/ReferenceCountedPointer.h>
#include <tl/optional.h>
#include <utility>

namespace Ichor {
    /// When a service has successfully started, this event gets added to inject it into other services
    struct DependencyOnlineEvent final : public Event {
        constexpr explicit DependencyOnlineEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority) noexcept :
                Event(_id, _originatingService, _priority) {}
        constexpr ~DependencyOnlineEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
            return TYPE;
        }

        static constexpr NameHashType TYPE = typeNameHash<DependencyOnlineEvent>();
        static constexpr std::string_view NAME = typeName<DependencyOnlineEvent>();
    };

    /// When a service should stop but before it has actually stopped, this event gets added to uninject it from other services
    struct DependencyOfflineEvent final : public Event {
        constexpr explicit DependencyOfflineEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority, bool _removeOriginatingServiceAfterStop) noexcept :
                Event(_id, _originatingService, _priority), removeOriginatingServiceAfterStop(_removeOriginatingServiceAfterStop) {}
        constexpr ~DependencyOfflineEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
            return TYPE;
        }

        bool removeOriginatingServiceAfterStop;
        static constexpr NameHashType TYPE = typeNameHash<DependencyOfflineEvent>();
        static constexpr std::string_view NAME = typeName<DependencyOfflineEvent>();
    };

    /// When a new service gets created that requests dependencies, each dependency it requests adds this event
    struct DependencyRequestEvent final : public Event {
        constexpr explicit DependencyRequestEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority, Dependency _dependency, tl::optional<v1::NeverNull<Properties const *>> _properties) noexcept :
                Event(_id, _originatingService, _priority), dependency(_dependency), properties{_properties} {}
        constexpr ~DependencyRequestEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
            return TYPE;
        }

        Dependency dependency;
        tl::optional<v1::NeverNull<Properties const *>> properties;
        static constexpr NameHashType TYPE = typeNameHash<DependencyRequestEvent>();
        static constexpr std::string_view NAME = typeName<DependencyRequestEvent>();
    };

    /// Similar to DependencyRequestEvent, but when a service gets destroyed/removed entirely
    /// Properties needs to be a copy as this event will be picked up after the service has been deleted from memory
    struct DependencyUndoRequestEvent final : public Event {
        constexpr explicit DependencyUndoRequestEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority, Dependency const &_dependency, tl::optional<Properties> const &_properties, v1::ReferenceCountedPointer<DependencyUndoRequestEvent> _originalRequest = {}) noexcept :
                Event(_id, _originatingService, _priority), dependency(_dependency), properties{_properties}, originalRequest(std::move(_originalRequest)) {}
        constexpr ~DependencyUndoRequestEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
            return TYPE;
        }

        Dependency dependency;
        tl::optional<Properties> properties;
        v1::ReferenceCountedPointer<DependencyUndoRequestEvent> originalRequest;
        static constexpr NameHashType TYPE = typeNameHash<DependencyUndoRequestEvent>();
        static constexpr std::string_view NAME = typeName<DependencyUndoRequestEvent>();
    };

    struct QuitEvent final : public Event {
        constexpr QuitEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority) noexcept : Event(_id, _originatingService, _priority) {}
        constexpr ~QuitEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
            return TYPE;
        }

        static constexpr NameHashType TYPE = typeNameHash<QuitEvent>();
        static constexpr std::string_view NAME = typeName<QuitEvent>();
    };

    struct StopServiceEvent final : public Event {
        constexpr StopServiceEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority, uint64_t _serviceId, bool _removeAfter = false) noexcept : Event(_id, _originatingService, _priority), serviceId(_serviceId), removeAfter(_removeAfter) {}
        constexpr ~StopServiceEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
            return TYPE;
        }

        ServiceIdType serviceId;
        bool removeAfter;
        static constexpr NameHashType TYPE = typeNameHash<StopServiceEvent>();
        static constexpr std::string_view NAME = typeName<StopServiceEvent>();
    };

    struct StartServiceEvent final : public Event {
        constexpr StartServiceEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority, uint64_t _serviceId) noexcept : Event(_id, _originatingService, _priority), serviceId(_serviceId) {}
        constexpr ~StartServiceEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
            return TYPE;
        }

        ServiceIdType serviceId;
        static constexpr NameHashType TYPE = typeNameHash<StartServiceEvent>();
        static constexpr std::string_view NAME = typeName<StartServiceEvent>();
    };

    struct RemoveServiceEvent final : public Event {
        constexpr RemoveServiceEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority, uint64_t _serviceId) noexcept : Event(_id, _originatingService, _priority), serviceId(_serviceId) {}
        constexpr ~RemoveServiceEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
            return TYPE;
        }

        ServiceIdType serviceId;
        static constexpr NameHashType TYPE = typeNameHash<RemoveServiceEvent>();
        static constexpr std::string_view NAME = typeName<RemoveServiceEvent>();
    };

    struct DoWorkEvent final : public Event {
        constexpr DoWorkEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority) noexcept : Event(_id, _originatingService, _priority) {}
        constexpr ~DoWorkEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
            return TYPE;
        }

        static constexpr NameHashType TYPE = typeNameHash<DoWorkEvent>();
        static constexpr std::string_view NAME = typeName<DoWorkEvent>();
    };

    struct RemoveEventHandlerEvent final : public Event {
        RemoveEventHandlerEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority, CallbackKey _key) noexcept : Event(_id, _originatingService, _priority), key(_key) {}
        ~RemoveEventHandlerEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
            return TYPE;
        }

        CallbackKey key;
        static constexpr NameHashType TYPE = typeNameHash<RemoveEventHandlerEvent>();
        static constexpr std::string_view NAME = typeName<RemoveEventHandlerEvent>();
    };

    struct RemoveEventInterceptorEvent final : public Event {
        constexpr RemoveEventInterceptorEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority, uint64_t _interceptorId, uint64_t _eventType) noexcept : Event(_id, _originatingService, _priority), interceptorId(_interceptorId), eventType(_eventType) {}
        constexpr ~RemoveEventInterceptorEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
            return TYPE;
        }

        uint64_t interceptorId;
        uint64_t eventType;
        static constexpr NameHashType TYPE = typeNameHash<RemoveEventInterceptorEvent>();
        static constexpr std::string_view NAME = typeName<RemoveEventInterceptorEvent>();
    };

    struct AddTrackerEvent final : public Event {
        constexpr AddTrackerEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority, NameHashType _interfaceNameHash) noexcept : Event(_id, _originatingService, _priority), interfaceNameHash(_interfaceNameHash) {}
        constexpr ~AddTrackerEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
            return TYPE;
        }

        NameHashType interfaceNameHash;
        static constexpr NameHashType TYPE = typeNameHash<AddTrackerEvent>();
        static constexpr std::string_view NAME = typeName<AddTrackerEvent>();
    };

    struct RemoveTrackerEvent final : public Event {
        constexpr RemoveTrackerEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority, NameHashType _interfaceNameHash) noexcept : Event(_id, _originatingService, _priority), interfaceNameHash(_interfaceNameHash) {}
        constexpr ~RemoveTrackerEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
            return TYPE;
        }

        NameHashType interfaceNameHash;
        static constexpr NameHashType TYPE = typeNameHash<RemoveTrackerEvent>();
        static constexpr std::string_view NAME = typeName<RemoveTrackerEvent>();
    };
}
