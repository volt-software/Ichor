#pragma once

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#include <synchapi.h>
#else
#include <pthread.h>
#endif

// Differs from std::mutex by setting some extra properties when creating the mutex
namespace Ichor {
    class RealtimeReadWriteMutex final {
    public:
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
        typedef PSRWLOCK native_handle_type;
#else
        typedef pthread_rwlock_t native_handle_type;
#endif

        RealtimeReadWriteMutex() noexcept {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
            ::InitializeSRWLock(native_handle());
#else
            ::pthread_rwlockattr_t attr;
            ::pthread_rwlockattr_init(&attr);
            ::pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
            ::pthread_rwlock_init(native_handle(), &attr);
            ::pthread_rwlockattr_destroy(&attr);
#endif
        }

        ~RealtimeReadWriteMutex() noexcept {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
            //nop
#else
            ::pthread_rwlock_destroy(native_handle());
#endif
        }

        void lock() noexcept {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
            ::AcquireSRWLockExclusive(native_handle());
#else
            ::pthread_rwlock_wrlock(native_handle());
#endif
        }

        bool try_lock() noexcept {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
            return ::TryAcquireSRWLockExclusive(native_handle()) != 0;
#else
            return ::pthread_rwlock_trywrlock(native_handle()) == 0;
#endif
        }

        void unlock() noexcept {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
            ::ReleaseSRWLockExclusive(native_handle());
#else
            ::pthread_rwlock_unlock(native_handle());
#endif
        }

        void lock_shared() noexcept {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
            ::AcquireSRWLockShared(native_handle());
#else
            ::pthread_rwlock_rdlock(native_handle());
#endif
        }

        bool try_lock_shared() noexcept {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
            return ::TryAcquireSRWLockShared(native_handle()) != 0;
#else
            return ::pthread_rwlock_tryrdlock(native_handle()) == 0;
#endif
        }

        void unlock_shared() noexcept {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
            ::ReleaseSRWLockExclusive(native_handle());
#else
            ::pthread_rwlock_unlock(native_handle());
#endif
        }

        native_handle_type* native_handle() noexcept {
            return &_rwlock;
        }

    private:
        native_handle_type _rwlock{};
    };
}