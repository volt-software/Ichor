#pragma once

#ifndef ICHOR_USE_LIBURING
#error "Ichor has not been compiled with io_uring support"
#endif

#include <ichor/event_queues/IEventQueue.h>
#include <ichor/event_queues/IIOUringQueue.h>
#include <atomic>
#include <thread>
#include <chrono>

namespace Ichor {
    /// Provides an io_uring based queue, expects the running OS to have at least kernel 6.1
    class IOUringQueue final : public IEventQueue, public IIOUringQueue {
    public:
        IOUringQueue(uint64_t quitTimeoutMs = 5'000, long long pollTimeoutNs = 100'000'000);
        ~IOUringQueue() final;

        void pushEventInternal(uint64_t priority, std::unique_ptr<Event> &&event) final;

        [[nodiscard]] bool empty() const final;
        [[nodiscard]] uint64_t size() const final;
        [[nodiscard]] bool is_running() const noexcept final;

        /// Creates an io_uring event loop with a standard flagset.
        /// \param entriesCount
        /// \return
        io_uring* createEventLoop(unsigned entriesCount = 2048);
        /// Use given ring.
        /// \param loop
        /// \param entriesCount entries count used when creating ring
        /// \return false if ring doesn't support features required by Ichor, true otherwise
        bool useEventLoop(io_uring *loop, unsigned entriesCount);

        bool start(bool captureSigInt) final;
        [[nodiscard]] bool shouldQuit() final;
        void quit() final;

        NeverNull<io_uring*> getRing() final;
        unsigned int getMaxEntriesCount() final;

        uint64_t getNextEventIdFromIchor() final;
        uint32_t sqeSpaceLeft() final;
        io_uring_sqe *getSqe() final;

        void submitIfNeeded() final;
        void submitAndWait(uint32_t waitNr) final;

    private:
        bool checkRingFlags(io_uring* ring);
        void shouldAddQuitEvent();

        std::unique_ptr<io_uring> _eventQueue{};
        io_uring *_eventQueuePtr{};
        std::atomic<bool> _quit{false};
        std::atomic<bool> _initializedQueue{false};
        std::thread::id _threadId{};
        uint64_t _quitTimeoutMs;
        unsigned int _entriesCount;
        long long _pollTimeoutNs;
        std::chrono::steady_clock::time_point _whenQuitEventWasSent{};
        std::atomic<bool> _quitEventSent{false};
    };
}
