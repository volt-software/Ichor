#pragma once

#include <ichor/events/Event.h>

namespace Ichor {
    struct CustomEvent final : public Event {
        explicit CustomEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority) noexcept :
                Event(_id, _originatingService, _priority) {}
        ~CustomEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR NameHashType get_type() const noexcept final {
            return TYPE;
        }

        static constexpr NameHashType TYPE = typeNameHash<CustomEvent>();
        static constexpr std::string_view NAME = typeName<CustomEvent>();
    };
}
