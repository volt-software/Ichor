#pragma once

#include <ichor/events/Event.h>
#include <ichor/ConstevalHash.h>
#include <optional>

namespace Ichor {
    struct ContinuableEvent final : public Event {
        ContinuableEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, uint64_t _promiseId) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority), promiseId(_promiseId) {}
        ~ContinuableEvent() final = default;

        uint64_t promiseId;
        static constexpr uint64_t TYPE = typeNameHash<ContinuableEvent>();
        static constexpr std::string_view NAME = typeName<ContinuableEvent>();
    };
}