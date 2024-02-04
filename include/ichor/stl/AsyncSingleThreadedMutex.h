#pragma once

#include <ichor/coroutines/Task.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <queue>
#include <memory>
#include <tl/optional.h>
#include <thread>

// Mutex that prevents access between coroutines on the same thread

namespace Ichor {
    class AsyncSingleThreadedMutex;

    class AsyncSingleThreadedLockGuard final {
        explicit AsyncSingleThreadedLockGuard(AsyncSingleThreadedMutex &m) : _m{&m} {
        }
    public:
        AsyncSingleThreadedLockGuard(const AsyncSingleThreadedLockGuard &) = delete;
        AsyncSingleThreadedLockGuard(AsyncSingleThreadedLockGuard &&o) noexcept : _m{o._m}, _has_lock(o._has_lock) {
            o._has_lock = false;
        }
        AsyncSingleThreadedLockGuard& operator=(const AsyncSingleThreadedLockGuard &o) = delete;
        AsyncSingleThreadedLockGuard& operator=(AsyncSingleThreadedLockGuard &&o) noexcept {
            _m = o._m;
            _has_lock = o._has_lock;
            o._has_lock = false;
            return *this;
        }
        ~AsyncSingleThreadedLockGuard();

        void unlock();
    private:
        AsyncSingleThreadedMutex *_m;
        bool _has_lock{true};
        friend class AsyncSingleThreadedMutex;
    };

    class AsyncSingleThreadedMutex final {
    public:
        Task<AsyncSingleThreadedLockGuard> lock() {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
            if(!_thread_id) {
                _thread_id = std::this_thread::get_id();
            } else if (_thread_id != std::this_thread::get_id()) {
                std::terminate();
            }
#endif
            if(_locked) {
                _evts.push(std::make_unique<AsyncManualResetEvent>());
                auto *evt = _evts.front().get();
                co_await *evt;
            }

            _locked = true;
            co_return AsyncSingleThreadedLockGuard{*this};
        }

        tl::optional<AsyncSingleThreadedLockGuard> non_blocking_lock() {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
            if(!_thread_id) {
                _thread_id = std::this_thread::get_id();
            } else if (_thread_id != std::this_thread::get_id()) {
                std::terminate();
            }
#endif

            if(_locked) {
                return tl::nullopt;
            }

            _locked = true;
            return AsyncSingleThreadedLockGuard{*this};
        }

        void unlock() {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
            if (_thread_id && _thread_id != std::this_thread::get_id()) {
                std::terminate();
            }
#endif
            if(_evts.empty()) {
                _locked = false;
                return;
            }

            auto evt = std::move(_evts.front());
            _evts.pop();
            evt->set();
        }
    private:
        bool _locked{};
        std::queue<std::unique_ptr<AsyncManualResetEvent>> _evts{};
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        tl::optional<std::thread::id> _thread_id;
#endif
    };
}
