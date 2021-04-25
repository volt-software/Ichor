#pragma once

#include <ichor/Events.h>

using namespace Ichor;

struct TestEvent final : public Event {
    explicit TestEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority) noexcept :
            Event(TYPE, NAME, _id, _originatingService, _priority) {}
    ~TestEvent() final = default;

    static constexpr uint64_t TYPE = typeNameHash<TestEvent>();
    static constexpr std::string_view NAME = typeName<TestEvent>();
};