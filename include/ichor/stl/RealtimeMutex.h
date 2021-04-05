#pragma once

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#include <synchapi.h>
#else
#include <pthread.h>
#endif

// Differs from std::mutex by setting some extra properties when creating the mutex
namespace Ichor {
    class RealtimeMutex final {
    public:
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
        typedef CRITICAL_SECTION native_handle_type;
#else
        typedef pthread_mutex_t native_handle_type;
#endif

        RealtimeMutex() noexcept {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
            ::InitializeCriticalSectionAndSpinCount(native_handle(), 100);
#else
            ::pthread_mutexattr_t attr;
            ::pthread_mutexattr_init(&attr);
            ::pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
            ::pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ADAPTIVE_NP);
            ::pthread_mutex_init(native_handle(), &attr);
            ::pthread_mutexattr_destroy(&attr);
#endif
        }

        ~RealtimeMutex() noexcept {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
            ::DeleteCriticalSection(native_handle());
#else
            ::pthread_mutex_destroy(native_handle());
#endif
        }

        void lock() noexcept {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
            ::EnterCriticalSection(native_handle());
#else
            ::pthread_mutex_lock(native_handle());
#endif
        }

        bool try_lock() noexcept {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
            return ::TryEnterCriticalSection(native_handle()) != 0;
#else
            return ::pthread_mutex_trylock(native_handle()) == 0;
#endif
        }

        void unlock() noexcept {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
            ::LeaveCriticalSection(native_handle());
#else
            ::pthread_mutex_unlock(native_handle());
#endif
        }

        native_handle_type* native_handle() noexcept {
            return &_mutex;
        }

    private:
        native_handle_type _mutex{};
    };
}