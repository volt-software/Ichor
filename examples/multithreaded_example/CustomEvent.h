#pragma once

#include <ichor/Events.h>

namespace Ichor {
    struct CustomEvent final : public Event {
        explicit CustomEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority) noexcept :
                Event(TYPE, NAME, _id, _originatingService, _priority) {}
        ~CustomEvent() final = default;

        static constexpr uint64_t TYPE = typeNameHash<CustomEvent>();
        static constexpr std::string_view NAME = typeName<CustomEvent>();
    };
}