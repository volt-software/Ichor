#pragma once

#include <ichor/events/Event.h>

namespace Ichor {
    struct CustomEvent final : public Event {
        explicit CustomEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority) noexcept :
                Event(_id, _originatingService, _priority) {}
        ~CustomEvent() final = default;

        [[nodiscard]] std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] uint64_t get_type() const noexcept final {
            return TYPE;
        }

        static constexpr uint64_t TYPE = typeNameHash<CustomEvent>();
        static constexpr std::string_view NAME = typeName<CustomEvent>();
    };
}
