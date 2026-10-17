#pragma once

#include <ichor/stl/RealtimeMutex.h>
#include <ichor/stl/ConditionVariable.h>
#include <chrono>
#include <ctime>
#include <mutex>

#if ICHOR_USE_STD_CONDITION_VARIABLE
#include <condition_variable>
#else
#include <pthread.h>
#endif

// Differs from std::condition_variable_any by working on steady clock only, using a reference instead of allocating memory and uses RealtimeMutex/RealtimeReadWriteMutex
namespace Ichor::v1 {
#if ICHOR_USE_STD_CONDITION_VARIABLE
    template <typename MutexT>
    using ConditionVariableAny = std::condition_variable_any;
#else

    template <typename MutexT>
    struct ConditionVariableAny final {
        explicit ConditionVariableAny() noexcept {
            pthread_cond_init(&_cond, nullptr);
        }

        ~ConditionVariableAny() noexcept {
            pthread_cond_destroy(&_cond);
        }

        // pthread condition variables may not be copied or moved
        ConditionVariableAny(const ConditionVariableAny&) = delete;
        ConditionVariableAny(ConditionVariableAny&&) = delete;
        ConditionVariableAny& operator=(const ConditionVariableAny&) = delete;
        ConditionVariableAny& operator=(ConditionVariableAny&&) = delete;

        void notify_all() noexcept
        {
            std::lock_guard const lock(_cvMutex);
            pthread_cond_broadcast(&_cond);
        }

        template<typename LockT, typename DurationT>
        cv_status wait_until(LockT& lock, const std::chrono::time_point<std::chrono::steady_clock, DurationT>& atime)
        {
            timespec const ts = Detail::toTimespec(atime);

            _cvMutex.lock();
            lock.unlock();
            pthread_cond_clockwait(&_cond, _cvMutex.native_handle(), CLOCK_MONOTONIC, &ts);
            _cvMutex.unlock();
            lock.lock();

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
        RealtimeMutex _cvMutex{};
        pthread_cond_t _cond{};
    };
#endif
}
