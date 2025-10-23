#pragma once

#include <ichor/events/Event.h>

using namespace Ichor;

struct TestEvent final : public Event {
    constexpr explicit TestEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority) noexcept :
            Event(_id, _originatingService, _priority) {}
    constexpr ~TestEvent() final = default;

    [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
        return NAME;
    }
    [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
        return TYPE;
    }

    static constexpr NameHashType TYPE = typeNameHash<TestEvent>();
    static constexpr std::string_view NAME = typeName<TestEvent>();
};

struct TestEvent2 final : public Event {
    constexpr explicit TestEvent2(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority) noexcept :
            Event(_id, _originatingService, _priority) {}
    constexpr ~TestEvent2() final = default;

    [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
        return NAME;
    }
    [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
        return TYPE;
    }

    static constexpr NameHashType TYPE = typeNameHash<TestEvent>();
    static constexpr std::string_view NAME = typeName<TestEvent>();
};
