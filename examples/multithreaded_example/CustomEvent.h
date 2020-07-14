#pragma once

#include "framework/Events.h"

namespace Cppelix {
    struct CustomEvent final : public Event {
        explicit CustomEvent(uint64_t _id, uint64_t _originatingService) noexcept :
                Event(TYPE, _id, _originatingService) {}
        ~CustomEvent() final = default;

        static constexpr uint64_t TYPE = typeNameHash<CustomEvent>();
    };
}