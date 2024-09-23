#pragma once

#include <ichor/services/timer/ITimer.h>

namespace Ichor {
    struct ITimerFactory {
        virtual ITimer& createTimer() = 0;
        virtual bool destroyTimerIfStopped(uint64_t timerId) = 0;

    protected:
        ~ITimerFactory() = default;
    };
}
