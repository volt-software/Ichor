#pragma once

#include <ichor/stl/RealtimeMutex.h>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <mutex>

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32) || defined(__APPLE__) || defined(ICHOR_MUSL)) && !defined(__CYGWIN__)
#include <condition_variable>
#define ICHOR_USE_STD_CONDITION_VARIABLE 1
#else
#include <pthread.h>
#define ICHOR_USE_STD_CONDITION_VARIABLE 0
#endif

// Differs from std::condition_variable by working on steady clock only, using a reference instead of allocating memory and uses RealtimeMutex rather than needing std::condition_variable_any
namespace Ichor::v1 {
#if ICHOR_USE_STD_CONDITION_VARIABLE
    using cv_status = std::cv_status;
    using ConditionVariable = std::condition_variable_any;
#else
    enum class cv_status : uint_fast16_t { no_timeout, timeout };

    namespace Detail {
        template<typename DurT>
        [[nodiscard]] constexpr timespec toTimespec(const std::chrono::time_point<std::chrono::steady_clock, DurT>& atime) noexcept {
            auto s = std::chrono::time_point_cast<std::chrono::seconds>(atime);
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(atime - s);

            return timespec{
                .tv_sec=static_cast<std::time_t>(s.time_since_epoch().count()),
                .tv_nsec=static_cast<long>(ns.count())
            };
        }

        /// Converts a relative timeout into an absolute steady_clock deadline, rounding up so that we never wait shorter than requested.
        template<typename RepT, typename PeriodT>
        [[nodiscard]] std::chrono::steady_clock::time_point deadlineFromNow(const std::chrono::duration<RepT, PeriodT>& rtime) noexcept {
            using durT = typename std::chrono::steady_clock::duration;
            auto reltime = std::chrono::duration_cast<durT>(rtime);
            if (reltime < rtime) {
                ++reltime;
            }
            return std::chrono::steady_clock::now() + reltime;
        }
    }

    struct ConditionVariable final {
        explicit ConditionVariable() noexcept {
            pthread_cond_init(&_cond, nullptr);
        }

        ~ConditionVariable() noexcept {
            pthread_cond_destroy(&_cond);
        }

        // pthread condition variables may not be copied or moved
        ConditionVariable(const ConditionVariable&) = delete;
        ConditionVariable(ConditionVariable&&) = delete;
        ConditionVariable& operator=(const ConditionVariable&) = delete;
        ConditionVariable& operator=(ConditionVariable&&) = delete;

        void notify_all() noexcept
        {
            pthread_cond_broadcast(&_cond);
        }

        template<typename DurationT>
        cv_status wait_until(std::unique_lock<RealtimeMutex>& lock, const std::chrono::time_point<std::chrono::steady_clock, DurationT>& atime)
        {
            timespec const ts = Detail::toTimespec(atime);

            pthread_cond_clockwait(&_cond, lock.mutex()->native_handle(), CLOCK_MONOTONIC, &ts);

            return (std::chrono::steady_clock::now() < atime
                    ? cv_status::no_timeout : cv_status::timeout);
        }

        template<typename LockT, typename RepT, typename PeriodT, typename PredicateT>
        bool wait_for(LockT& lock, const std::chrono::duration<RepT, PeriodT>& rtime, PredicateT pred) {
            // The deadline is computed once, spurious wakeups or notifies must not extend the total waiting time.
            auto const deadline = Detail::deadlineFromNow(rtime);
            while (!pred()) {
                if (wait_until(lock, deadline) == cv_status::timeout) {
                    return pred();
                }
            }
            return true;
        }

    private:
        pthread_cond_t _cond{};
    };
#endif
}
