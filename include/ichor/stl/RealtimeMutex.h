#pragma once

#include <ichor/Defines.h>
#include <exception>

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
#include <windows.h>
#include <synchapi.h>
#else
#include <pthread.h>
#endif

// Differs from std::mutex by setting some extra properties when creating the mutex (priority inheritance, adaptive spinning)
namespace Ichor::v1 {
    class RealtimeMutex final {
    public:
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
        typedef CRITICAL_SECTION native_handle_type;
#else
        using native_handle_type = pthread_mutex_t;
#endif

        RealtimeMutex() noexcept {
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
            ::InitializeCriticalSectionAndSpinCount(native_handle(), 100);
#else
            ::pthread_mutexattr_t attr;
            if(::pthread_mutexattr_init(&attr) != 0) [[unlikely]] {
                std::terminate();
            }
            // Priority inheritance is best-effort, not all platforms support it.
            static_cast<void>(::pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT));

#if !defined(__APPLE__) && !defined(ICHOR_MUSL)
            static_cast<void>(::pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ADAPTIVE_NP));
#endif

            if(::pthread_mutex_init(native_handle(), &attr) != 0) [[unlikely]] {
                std::terminate();
            }
            ::pthread_mutexattr_destroy(&attr);
#endif
        }

        ~RealtimeMutex() noexcept {
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
            ::DeleteCriticalSection(native_handle());
#else
            ::pthread_mutex_destroy(native_handle());
#endif
        }

        // The native mutex may not be copied or moved
        RealtimeMutex(const RealtimeMutex&) = delete;
        RealtimeMutex(RealtimeMutex&&) = delete;
        RealtimeMutex& operator=(const RealtimeMutex&) = delete;
        RealtimeMutex& operator=(RealtimeMutex&&) = delete;

        void lock() noexcept {
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
            ::EnterCriticalSection(native_handle());
#else
            [[maybe_unused]] auto const ret = ::pthread_mutex_lock(native_handle());
            if constexpr (DO_HARDENING) {
                if(ret != 0) [[unlikely]] {
                    std::terminate();
                }
            }
#endif
        }

        bool try_lock() noexcept {
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
            return ::TryEnterCriticalSection(native_handle()) != 0;
#else
            return ::pthread_mutex_trylock(native_handle()) == 0;
#endif
        }

        void unlock() noexcept {
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
            ::LeaveCriticalSection(native_handle());
#else
            [[maybe_unused]] auto const ret = ::pthread_mutex_unlock(native_handle());
            if constexpr (DO_HARDENING) {
                if(ret != 0) [[unlikely]] {
                    std::terminate();
                }
            }
#endif
        }

        native_handle_type* native_handle() noexcept {
            return &_mutex;
        }

    private:
        native_handle_type _mutex{};
    };
}
