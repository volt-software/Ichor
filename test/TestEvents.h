#pragma once

#include <ichor/events/Event.h>

using namespace Ichor;

struct TestEvent final : public Event {
    explicit TestEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority) noexcept :
            Event(_id, _originatingService, _priority) {}
    ~TestEvent() final = default;

    [[nodiscard]] std::string_view get_name() const noexcept final {
        return NAME;
    }
    [[nodiscard]] uint64_t get_type() const noexcept final {
        return TYPE;
    }

    static constexpr uint64_t TYPE = typeNameHash<TestEvent>();
    static constexpr std::string_view NAME = typeName<TestEvent>();
};

struct TestEvent2 final : public Event {
    explicit TestEvent2(uint64_t _id, uint64_t _originatingService, uint64_t _priority) noexcept :
            Event(_id, _originatingService, _priority) {}
    ~TestEvent2() final = default;

    [[nodiscard]] std::string_view get_name() const noexcept final {
        return NAME;
    }
    [[nodiscard]] uint64_t get_type() const noexcept final {
        return TYPE;
    }

    static constexpr uint64_t TYPE = typeNameHash<TestEvent>();
    static constexpr std::string_view NAME = typeName<TestEvent>();
};
