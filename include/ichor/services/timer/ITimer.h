#pragma once

#include <ichor/coroutines/AsyncGenerator.h>
#include <functional>

namespace Ichor {
    enum class TimerState : uint_fast16_t {
        STOPPED,
        STARTING,
        RUNNING,
        STOPPING
    };

    struct ITimer {

        /// Start timer
        /// \return false if timer could not start (maybe already running or currently stopping/starting), true otherwise
        virtual bool startTimer() = 0;

        /// Start timer
        /// \param fireImmediately Do not wait for given interval for first execution
        /// \return false if timer could not start (maybe already running or currently stopping/starting), true otherwise
        virtual bool startTimer(bool fireImmediately) = 0;

        /// \param cb called when timer is stopped, after which it is safe to destroy. Also gets called if timer already stopping/stopped.
        /// \return false if timer could not stop (maybe already stopped or currently stopping/starting), true otherwise
        virtual bool stopTimer(std::function<void(void)> cb) = 0;

        [[nodiscard]] virtual TimerState getState() const noexcept = 0;

        /// \param nanoseconds
        virtual void setInterval(uint64_t nanoseconds) noexcept = 0;

        virtual void setPriority(uint64_t priority) noexcept = 0;
        [[nodiscard]] virtual uint64_t getPriority() const noexcept = 0;

        /// \param fireOnce
        virtual void setFireOnce(bool fireOnce) noexcept = 0;
        [[nodiscard]] virtual bool getFireOnce() const noexcept = 0;

        [[nodiscard]] virtual uint64_t getTimerId() const noexcept = 0;

        [[nodiscard]] virtual ServiceIdType getRequestingServiceId() const noexcept = 0;


        /// Sets coroutine based callback, adds some overhead compared to sync version. Executed when timer expires. Terminates program if timer is running.
        /// \param fn callback
        virtual void setCallbackAsync(std::function<AsyncGenerator<IchorBehaviour>()> fn) = 0;

        /// Set sync callback to execute when timer expires. Terminates program if timer is running.
        /// \param fn callback
        virtual void setCallback(std::function<void()> fn) = 0;

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

template <>
struct fmt::formatter<Ichor::TimerState> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::TimerState& change, FormatContext& ctx) {
        switch(change) {
            case Ichor::TimerState::STOPPED:
                return fmt::format_to(ctx.out(), "STOPPED");
            case Ichor::TimerState::STARTING:
                return fmt::format_to(ctx.out(), "STARTING");
            case Ichor::TimerState::RUNNING:
                return fmt::format_to(ctx.out(), "RUNNING");
            case Ichor::TimerState::STOPPING:
                return fmt::format_to(ctx.out(), "STOPPING");
        }
        return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
    }
};
