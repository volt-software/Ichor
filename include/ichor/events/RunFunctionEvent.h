#pragma once

#include <ichor/events/Event.h>
#include <ichor/ConstevalHash.h>
#include <ichor/coroutines/AsyncGenerator.h>

namespace Ichor {
    struct RunFunctionEventAsync final : public Event {
        RunFunctionEventAsync(uint64_t _id, uint64_t _originatingService, uint64_t _priority, std::function<AsyncGenerator<IchorBehaviour>()> _fun) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority), fun(std::move(_fun)) {}
        ~RunFunctionEventAsync() final = default;

        std::function<AsyncGenerator<IchorBehaviour>()> fun;
        static constexpr uint64_t TYPE = typeNameHash<RunFunctionEventAsync>();
        static constexpr std::string_view NAME = typeName<RunFunctionEventAsync>();
    };

    struct RunFunctionEvent final : public Event {
        RunFunctionEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, std::function<void()> _fun) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority), fun(std::move(_fun)) {}
        ~RunFunctionEvent() final = default;

        std::function<void()> fun;
        static constexpr uint64_t TYPE = typeNameHash<RunFunctionEvent>();
        static constexpr std::string_view NAME = typeName<RunFunctionEvent>();
    };
}
