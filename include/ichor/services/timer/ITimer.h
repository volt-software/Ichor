#pragma once

#include <ichor/events/InternalEvents.h>

namespace Ichor {
    struct ITimer {
        virtual void startTimer() = 0;
        virtual void startTimer(bool fireImmediately) = 0;
        virtual void stopTimer() = 0;
        [[nodiscard]] virtual bool running() const noexcept = 0;
        virtual void setInterval(uint64_t nanoseconds) noexcept = 0;
        virtual void setPriority(uint64_t priority) noexcept = 0;
        [[nodiscard]] virtual uint64_t getPriority() const noexcept = 0;

        template <typename Dur>
        void setChronoInterval(Dur duration) noexcept {
            int64_t val = std::chrono::nanoseconds(duration).count();

#ifdef ICHOR_USE_HARDENING
            if(val < 0) [[unlikely]] {
                std::terminate();
            }
#endif

            setInterval(static_cast<uint64_t>(val));
        }

    protected:
        ~ITimer() = default;
    };
}