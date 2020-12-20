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
        virtual void startTimer() = 0;
        virtual void stopTimer() = 0;
        [[nodiscard]] virtual bool running() const noexcept = 0;
        virtual void setInterval(uint64_t nanoseconds) noexcept = 0;
        virtual void setPriority(uint64_t priority) noexcept = 0;
        [[nodiscard]] virtual uint64_t getPriority() const noexcept = 0;

        template <typename Dur>
        void setChronoInterval(Dur duration) noexcept {
            setInterval(std::chrono::nanoseconds(duration).count());
        }
    };
}