#pragma once

#include <ichor/services/timer/ITimer.h>
#include <ichor/stl/NeverAlwaysNull.h>
#include <ichor/stl/Spans.h>
#include <tl/optional.h>

namespace Ichor::v1 {
    // NOLINTNEXTLINE(cppcoreguidelines-virtual-class-destructor): final, and ITimer's destructor is protected: nothing deletes it through another type
    class TimerRef; // forward declaration to avoid circular include
    struct ITimerFactory {
        /// The timer lives as long as a TimerRef to it exists, see TimerRef.
        [[nodiscard]] virtual TimerRef createTimer() = 0;
        // Retrieve a timer by its id. Returns nullptr when not found.
        [[nodiscard]] virtual tl::optional<NeverNull<ITimer*>> getTimerById(uint64_t timerId) noexcept = 0;
        /// Using a TimerRef to the timer after this destroyed it terminates the program.
        virtual bool destroyTimerIfStopped(uint64_t timerId) = 0;
        /// Stops the timer and destroys it once it has stopped. Called when the last TimerRef to a timer is destroyed.
        virtual void releaseTimer(uint64_t timerId) noexcept = 0;
        /// View over the timers of the requesting service, without copying them. The view is invalidated by any timer being created or
        /// destroyed: using it after createTimer(), destroyTimerIfStopped(), destroying the last TimerRef of a timer (which may destroy it
        /// later, from a stop callback) or the factory stopping is undefined behaviour. Use it right away, don't keep it around.
        [[nodiscard]] virtual VectorView<ITimer> getCreatedTimers() const noexcept = 0;
        [[nodiscard]] virtual ServiceIdType getRequestingServiceId() const noexcept = 0;

    protected:
        ~ITimerFactory() = default;
    };
}

#include <ichor/services/timer/TimerRef.h>
