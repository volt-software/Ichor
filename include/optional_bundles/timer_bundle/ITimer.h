#pragma once

#include "framework/Events.h"

namespace Cppelix {
    struct TimerEvent final : public Event {
        TimerEvent(uint64_t _id, uint64_t _originatingService, uint64_t _timerId) noexcept : Event(TYPE, _id, _originatingService), timerId(_timerId) {}
        ~TimerEvent() final = default;

        const uint64_t timerId;
        static constexpr uint64_t TYPE = typeNameHash<TimerEvent>();
    };

    struct ITimer : virtual public IService {
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};

        virtual bool running() const = 0;
    };
}