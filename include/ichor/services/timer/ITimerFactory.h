#pragma once

#include <ichor/services/timer/ITimer.h>
#include <ichor/stl/VectorView.h>

namespace Ichor::v1 {
    class TimerRef; // forward declaration to avoid circular include
    struct ITimerFactory {
        [[nodiscard]] virtual TimerRef createTimer() = 0;
        // Retrieve a timer by its id. Returns nullptr when not found.
        [[nodiscard]] virtual tl::optional<NeverNull<ITimer*>> getTimerById(uint64_t timerId) noexcept = 0;
        virtual bool destroyTimerIfStopped(uint64_t timerId) = 0;
        [[nodiscard]] virtual VectorView<ITimer> getCreatedTimers() const noexcept = 0;
        [[nodiscard]] virtual ServiceIdType getRequestingServiceId() const noexcept = 0;

    protected:
        ~ITimerFactory() = default;
    };
}

#include <ichor/services/timer/TimerRef.h>
