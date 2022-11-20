#pragma once

#include <cstdint>
#include <string_view>

namespace Ichor {
    constexpr uint64_t INTERNAL_EVENT_PRIORITY = 1000;
    constexpr uint64_t INTERNAL_DEPENDENCY_EVENT_PRIORITY = 100; // only go below if you know what you're doing
    constexpr uint64_t INTERNAL_COROUTINE_EVENT_PRIORITY = 98; // only go below if you know what you're doing
    constexpr uint64_t INTERNAL_INSERT_SERVICE_EVENT_PRIORITY = 50; // only go below if you know what you're doing

    struct Event {
        Event(uint64_t _type, std::string_view _name, uint64_t _id, uint64_t _originatingService, uint64_t _priority) noexcept : type{_type}, name{_name}, id{_id}, originatingService{_originatingService}, priority{_priority} {}
        Event(const Event &) = default;
        Event(Event &&) noexcept = default;
        Event &operator=(const Event &) = default;
        Event &operator=(Event &&) noexcept = default;
        virtual ~Event() = default;
        uint64_t type;
        std::string_view name;
        uint64_t id;
        uint64_t originatingService;
        uint64_t priority;
    };
}