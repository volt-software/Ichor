#pragma once

#include <ichor/events/Event.h>
#include <ichor/ConstevalHash.h>
#include <ichor/DependencyManager.h>
#include <optional>

namespace Ichor {
    struct RunFunctionEvent final : public Event {
        RunFunctionEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, std::function<AsyncGenerator<void>(DependencyManager&)> _fun) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority), fun(std::move(_fun)) {}
        ~RunFunctionEvent() final = default;

        std::function<AsyncGenerator<void>(DependencyManager&)> fun;
        static constexpr uint64_t TYPE = typeNameHash<RunFunctionEvent>();
        static constexpr std::string_view NAME = typeName<RunFunctionEvent>();
    };
}