#pragma once

#include <ichor/events/Event.h>
#include <ichor/ConstevalHash.h>

namespace Ichor {
    struct Dependency;

    struct ContinuableEvent final : public Event {
        constexpr ContinuableEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority, uint64_t _promiseId) noexcept : Event(_id, _originatingService, _priority), promiseId(_promiseId) {}
        constexpr ~ContinuableEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
            return TYPE;
        }

        uint64_t promiseId;
        static constexpr NameHashType TYPE = typeNameHash<ContinuableEvent>();
        static constexpr std::string_view NAME = typeName<ContinuableEvent>();
    };

    struct ContinuableStartEvent final : public Event {
        constexpr ContinuableStartEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority, uint64_t _promiseId) noexcept : Event(_id, _originatingService, _priority), promiseId(_promiseId) {}
        constexpr ~ContinuableStartEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
            return TYPE;
        }

        uint64_t promiseId;
        static constexpr NameHashType TYPE = typeNameHash<ContinuableStartEvent>();
        static constexpr std::string_view NAME = typeName<ContinuableStartEvent>();
    };

    struct ContinuableDependencyOfflineEvent final : public Event {
        explicit ContinuableDependencyOfflineEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority, ServiceIdType _originatingOfflineServiceId, bool _removeOriginatingOfflineServiceAfterStop, std::vector<Dependency *> _dependencyIterators) noexcept :
                Event(_id, _originatingService, _priority), originatingOfflineServiceId(_originatingOfflineServiceId), removeOriginatingOfflineServiceAfterStop(_removeOriginatingOfflineServiceAfterStop), dependencyIterators(std::move(_dependencyIterators)) {}
        ~ContinuableDependencyOfflineEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
            return TYPE;
        }

        ServiceIdType originatingOfflineServiceId;
        bool removeOriginatingOfflineServiceAfterStop;
        std::vector<Dependency *> dependencyIterators;
        static constexpr NameHashType TYPE = typeNameHash<ContinuableDependencyOfflineEvent>();
        static constexpr std::string_view NAME = typeName<ContinuableDependencyOfflineEvent>();
    };
}
