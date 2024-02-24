#pragma once

#ifdef ICHOR_USE_LIBURING

#include <ichor/event_queues/IEventQueue.h>
#include <atomic>
#include <thread>
#include <chrono>

struct io_uring;

namespace Ichor {
    /// Provides an io_uring based queue, expects the running OS to have at least kernel 6.1
    class IOUringQueue final : public IEventQueue {
    public:
        IOUringQueue(uint64_t quitTimeoutMs = 5'000, long long pollTimeoutNs = 100'000'000);
        ~IOUringQueue() final;

        void pushEventInternal(uint64_t priority, std::unique_ptr<Event> &&event) final;

        [[nodiscard]] bool empty() const final;
        [[nodiscard]] uint64_t size() const final;
        [[nodiscard]] bool is_running() const noexcept final;

        io_uring* createEventLoop(unsigned entriesCount = 2048);
        void useEventLoop(io_uring *loop, unsigned entriesCount);

        void start(bool captureSigInt) final;
        [[nodiscard]] bool shouldQuit() final;
        void quit() final;

    private:
        void shouldAddQuitEvent();

        std::unique_ptr<io_uring> _eventQueue{};
        io_uring *_eventQueuePtr{};
        std::atomic<bool> _quit{false};
        std::atomic<bool> _initializedQueue{false};
        std::thread::id _threadId{};
        uint64_t _quitTimeoutMs;
        int _entriesCount;
        long long _pollTimeoutNs;
        std::chrono::steady_clock::time_point _whenQuitEventWasSent{};
        std::atomic<bool> _quitEventSent{false};
    };
}

#endif
