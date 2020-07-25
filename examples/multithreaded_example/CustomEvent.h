#pragma once

#include "framework/Events.h"

namespace Cppelix {
    struct CustomEvent final : public Event {
        explicit CustomEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority) noexcept :
                Event(TYPE, _id, _originatingService, _priority) {}
        ~CustomEvent() final = default;

        static constexpr uint64_t TYPE = typeNameHash<CustomEvent>();
    };
}