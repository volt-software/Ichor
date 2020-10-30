#pragma once

#include <ichor/Events.h>

namespace Ichor {
    struct TimerEvent final : public Event {
        TimerEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority) {}
        ~TimerEvent() final = default;

        static constexpr uint64_t TYPE = typeNameHash<TimerEvent>();
        static constexpr std::string_view NAME = typeName<TimerEvent>();
    };

    struct ITimer : virtual public IService {
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};

        virtual void startTimer() = 0;
        virtual void stopTimer() = 0;
        [[nodiscard]] virtual bool running() const = 0;
        virtual void setInterval(uint64_t nanoseconds) = 0;
        virtual void setPriority(uint64_t priority) = 0;
        virtual uint64_t getPriority() const = 0;

        template <typename Dur>
        void setChronoInterval(Dur duration) {
            setInterval(std::chrono::nanoseconds(duration).count());
        }
    };
}