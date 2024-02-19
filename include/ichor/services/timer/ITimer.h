#pragma once

#include <ichor/coroutines/AsyncGenerator.h>

namespace Ichor {
    struct ITimer {
        /// Thread-safe.
        virtual void startTimer() = 0;
        /// Thread-safe.
        virtual void startTimer(bool fireImmediately) = 0;
        /// Thread-safe.
        virtual void stopTimer() = 0;
        /// Thread-safe.
        [[nodiscard]] virtual bool running() const noexcept = 0;
        /// Thread-safe.
        /// \param nanoseconds
        virtual void setInterval(uint64_t nanoseconds) noexcept = 0;
        /// Thread-safe.
        virtual void setPriority(uint64_t priority) noexcept = 0;
        /// Thread-safe.
        [[nodiscard]] virtual uint64_t getPriority() const noexcept = 0;
        /// Thread-safe.
        /// \param fireOnce
        virtual void setFireOnce(bool fireOnce) noexcept = 0;
        /// Thread-safe.
        [[nodiscard]] virtual bool getFireOnce() const noexcept = 0;
        /// Thread-safe.
        [[nodiscard]] virtual uint64_t getTimerId() const noexcept = 0;


        /// Sets coroutine based callback, adds some overhead compared to sync version. Executed when timer expires. Terminates program if timer is running.
        /// \param fn callback
        virtual void setCallbackAsync(std::function<AsyncGenerator<IchorBehaviour>()> fn) = 0;

        /// Set sync callback to execute when timer expires. Terminates program if timer is running.
        /// \param fn callback
        virtual void setCallback(std::function<void()> fn) = 0;

        /// Thread-safe.
        /// \tparam Dur std::chrono type
        /// \param duration
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
