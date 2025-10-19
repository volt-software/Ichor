#pragma once

#include <ichor/events/Event.h>
#include <ichor/ConstevalHash.h>
#include <tl/optional.h>

namespace Ichor {
    struct ContinuableEvent final : public Event {
        ContinuableEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, uint64_t _promiseId) noexcept : Event(_id, _originatingService, _priority), promiseId(_promiseId) {}
        ~ContinuableEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR NameHashType get_type() const noexcept final {
            return TYPE;
        }

        uint64_t promiseId;
        static constexpr NameHashType TYPE = typeNameHash<ContinuableEvent>();
        static constexpr std::string_view NAME = typeName<ContinuableEvent>();
    };

    struct ContinuableStartEvent final : public Event {
        ContinuableStartEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, uint64_t _promiseId) noexcept : Event(_id, _originatingService, _priority), promiseId(_promiseId) {}
        ~ContinuableStartEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR NameHashType get_type() const noexcept final {
            return TYPE;
        }

        uint64_t promiseId;
        static constexpr NameHashType TYPE = typeNameHash<ContinuableStartEvent>();
        static constexpr std::string_view NAME = typeName<ContinuableStartEvent>();
    };

    struct ContinuableDependencyOfflineEvent final : public Event {
        explicit ContinuableDependencyOfflineEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, uint64_t _originatingOfflineServiceId, bool _removeOriginatingOfflineServiceAfterStop) noexcept :
                Event(_id, _originatingService, _priority), originatingOfflineServiceId(_originatingOfflineServiceId), removeOriginatingOfflineServiceAfterStop(_removeOriginatingOfflineServiceAfterStop) {}
        ~ContinuableDependencyOfflineEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR NameHashType get_type() const noexcept final {
            return TYPE;
        }

        uint64_t originatingOfflineServiceId;
        bool removeOriginatingOfflineServiceAfterStop;
        static constexpr NameHashType TYPE = typeNameHash<ContinuableDependencyOfflineEvent>();
        static constexpr std::string_view NAME = typeName<ContinuableDependencyOfflineEvent>();
    };
}
