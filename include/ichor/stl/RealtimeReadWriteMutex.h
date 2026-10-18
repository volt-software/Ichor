#pragma once

#include <ichor/Defines.h>
#include <exception>

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
#include <windows.h>
#include <synchapi.h>
#else
#include <pthread.h>
#endif

// Differs from std::shared_mutex by preferring writers when creating the lock, so that writers cannot be starved by readers.
// Because of that, a thread holding a shared lock must not try to acquire another shared lock, it may deadlock if a writer is waiting.
namespace Ichor::v1 {
    class RealtimeReadWriteMutex final {
    public:
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
        typedef SRWLOCK native_handle_type;
#else
        using native_handle_type = pthread_rwlock_t;
#endif

        RealtimeReadWriteMutex() noexcept {
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
            ::InitializeSRWLock(&_rwlock);
#else
            ::pthread_rwlockattr_t attr;
            if(::pthread_rwlockattr_init(&attr) != 0) [[unlikely]] {
                std::terminate();
            }

#if !defined(__APPLE__) && !defined(ICHOR_MUSL)
            static_cast<void>(::pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP));
#endif

            if(::pthread_rwlock_init(native_handle(), &attr) != 0) [[unlikely]] {
                std::terminate();
            }
            ::pthread_rwlockattr_destroy(&attr);
#endif
        }

        ~RealtimeReadWriteMutex() noexcept {
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
            //nop
#else
            ::pthread_rwlock_destroy(native_handle());
#endif
        }

        // The native lock may not be copied or moved
        RealtimeReadWriteMutex(const RealtimeReadWriteMutex&) = delete;
        RealtimeReadWriteMutex(RealtimeReadWriteMutex&&) = delete;
        RealtimeReadWriteMutex& operator=(const RealtimeReadWriteMutex&) = delete;
        RealtimeReadWriteMutex& operator=(RealtimeReadWriteMutex&&) = delete;

        void lock() noexcept {
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
            ::AcquireSRWLockExclusive(&_rwlock);
#else
            checkResult(::pthread_rwlock_wrlock(native_handle()));
#endif
        }

        bool try_lock() noexcept {
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
            return ::TryAcquireSRWLockExclusive(&_rwlock) != 0;
#else
            return ::pthread_rwlock_trywrlock(native_handle()) == 0;
#endif
        }

        void unlock() noexcept {
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
            ::ReleaseSRWLockExclusive(&_rwlock);
#else
            checkResult(::pthread_rwlock_unlock(native_handle()));
#endif
        }

        void lock_shared() noexcept {
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
            ::AcquireSRWLockShared(&_rwlock);
#else
            checkResult(::pthread_rwlock_rdlock(native_handle()));
#endif
        }

        bool try_lock_shared() noexcept {
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
            return ::TryAcquireSRWLockShared(&_rwlock) != 0;
#else
            return ::pthread_rwlock_tryrdlock(native_handle()) == 0;
#endif
        }

        void unlock_shared() noexcept {
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
            ::ReleaseSRWLockShared(&_rwlock);
#else
            checkResult(::pthread_rwlock_unlock(native_handle()));
#endif
        }

        native_handle_type* native_handle() noexcept {
            return &_rwlock;
        }

    private:
        static void checkResult([[maybe_unused]] int ret) noexcept {
            if constexpr (DO_HARDENING) {
                if(ret != 0) [[unlikely]] {
                    std::terminate();
                }
            }
        }

        native_handle_type _rwlock{};
    };
}
