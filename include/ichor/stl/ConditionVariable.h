#pragma once

#include <ichor/stl/RealtimeMutex.h>

// Differs from std::condition_variable by working on steady clock only, using a reference instead of allocating memory and uses RealtimeMutex rather than needing std::condition_variable_any
namespace Ichor {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
    using ConditionVariable = std::condition_variable_any;
#else
    enum class cv_status { no_timeout, timeout };
    
    struct ConditionVariable final {
        explicit ConditionVariable(RealtimeMutex &m) noexcept : _m(m) {
            pthread_cond_init(&_cond, nullptr);
        }

        ~ConditionVariable() noexcept {
            pthread_cond_destroy(&_cond);
        }

        void notify_all() noexcept
        {
            std::lock_guard<RealtimeMutex> lock(_m);
            pthread_cond_broadcast(&_cond);
        }

        template<typename DurT>
        cv_status _wait_until_impl(std::unique_lock<RealtimeMutex>& lock, const std::chrono::time_point<std::chrono::steady_clock, DurT>& atime)
        {
            auto _s = std::chrono::time_point_cast<std::chrono::seconds>(atime);
            auto _ns = std::chrono::duration_cast<std::chrono::nanoseconds>(atime - _s);

            timespec _ts = {
                static_cast<std::time_t>(_s.time_since_epoch().count()),
                static_cast<long>(_ns.count())
            };

            pthread_cond_clockwait(&_cond, lock.mutex()->native_handle(), CLOCK_MONOTONIC, &_ts);

            return (std::chrono::steady_clock::now() < atime
                    ? cv_status::no_timeout : cv_status::timeout);
        }

        template<typename DurationT>
        cv_status wait_until(std::unique_lock<RealtimeMutex>& lock, const std::chrono::time_point<std::chrono::steady_clock, DurationT>& atime)
        {
            static_assert(std::chrono::is_clock_v<std::chrono::steady_clock>);
            const typename std::chrono::steady_clock::time_point _c_entry = std::chrono::steady_clock::now();
            const std::chrono::steady_clock::time_point _s_entry = std::chrono::steady_clock::now();
            const auto _delta = atime - _c_entry;
            const auto _s_atime = _s_entry + _delta;

            if (_wait_until_impl(lock, _s_atime) == cv_status::no_timeout)
                return cv_status::no_timeout;
            // We got a timeout when measured against __clock_t but
            // we need to check against the caller-supplied clock
            // to tell whether we should return a timeout.
            if (std::chrono::steady_clock::now() < atime)
                return cv_status::no_timeout;
            return cv_status::timeout;
        }

        template<typename LockT, typename RepT, typename PeriodT, typename PredicateT>
        bool wait_for(LockT& lock, const std::chrono::duration<RepT, PeriodT>& rtime, PredicateT pred) {
            using durT = typename std::chrono::steady_clock::duration;
            auto reltime = std::chrono::duration_cast<durT>(rtime);
            if (reltime < rtime)
                ++reltime;
            while (!pred())
                if (wait_until(lock, std::chrono::steady_clock::now() + reltime) == cv_status::timeout)
                    return pred();
            return true;
        }

    private:
        RealtimeMutex &_m;
        pthread_cond_t _cond{};
    };
#endif
}