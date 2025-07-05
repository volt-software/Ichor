#pragma once

#include <ichor/services/timer/ITimer.h>
#include <vector>

namespace Ichor::v1 {
    struct ITimerFactory {
        [[nodiscard]] virtual ITimer& createTimer() = 0;
        virtual bool destroyTimerIfStopped(uint64_t timerId) = 0;
        [[nodiscard]] virtual std::vector<NeverNull<ITimer*>> getCreatedTimers() const noexcept = 0;

    protected:
        ~ITimerFactory() = default;
    };
}
