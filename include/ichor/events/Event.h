#pragma once

#include <cstdint>
#include <string_view>

namespace Ichor {
    constexpr uint64_t INTERNAL_EVENT_PRIORITY = 1000;
    constexpr uint64_t INTERNAL_DEPENDENCY_EVENT_PRIORITY = 100; // only go below 100 if you know what you're doing

    struct Event {
        Event(uint64_t _type, std::string_view _name, uint64_t _id, uint64_t _originatingService, uint64_t _priority) noexcept : type{_type}, name{_name}, id{_id}, originatingService{_originatingService}, priority{_priority} {}
        virtual ~Event() = default;
        const uint64_t type;
        const std::string_view name;
        const uint64_t id;
        const uint64_t originatingService;
        const uint64_t priority;
    };
}