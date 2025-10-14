#pragma once

#ifndef ICHOR_USE_LIBURING
#error "Ichor has not been compiled with io_uring support"
#endif

#include <ichor/event_queues/IIOUringQueue.h>
#include <ichor/ichor_liburing.h>
#include <atomic>
#include <thread>
#include <chrono>

namespace Ichor {
    /// Provides an io_uring based queue, expects the running OS to have at least kernel 5.4, but multithreading support is only available from 5.18 and later.
    class IOUringQueue final : public IIOUringQueue {
    public:
        IOUringQueue(uint64_t quitTimeoutMs = 5'000, long long pollTimeoutNs = 100'000'000, tl::optional<v1::Version> emulateKernelVersion = {});
        ~IOUringQueue() final;

        void pushEventInternal(uint64_t priority, std::unique_ptr<Event> &&event) final;

        [[nodiscard]] bool empty() const final;
        [[nodiscard]] uint64_t size() const final;
        [[nodiscard]] bool is_running() const noexcept final;
        [[nodiscard]] NameHashType get_queue_name_hash() const noexcept final;

        /// Creates an io_uring event loop with a standard flagset.
        /// \param entriesCount
        /// \return
        [[nodiscard]] tl::optional<v1::NeverNull<io_uring*>> createEventLoop(unsigned entriesCount = 2048);
        /// Use given ring.
        /// \param loop
        /// \param entriesCount entries count used when creating ring
        /// \return false if ring doesn't support features required by Ichor, true otherwise
        bool useEventLoop(v1::NeverNull<io_uring*> loop, unsigned entriesCount);

        bool start(bool captureSigInt) final;
        [[nodiscard]] bool shouldQuit() final;
        void quit() final;

        [[nodiscard]] v1::NeverNull<io_uring*> getRing() noexcept final;
        [[nodiscard]] unsigned int getMaxEntriesCount() const noexcept final;

        [[nodiscard]] uint32_t sqeSpaceLeft() const noexcept final;
        [[nodiscard]] io_uring_sqe *getSqe() noexcept final;
        io_uring_sqe* getSqeWithData(IService *self, std::function<void(io_uring_cqe*)> fun) noexcept final;
        io_uring_sqe* getSqeWithData(ServiceIdType serviceId, std::function<void(io_uring_cqe*)> fun) noexcept final;

        void submitIfNeeded() final;
        void forceSubmit() final;
        void submitAndWait(uint32_t waitNr) final;

        [[nodiscard]] v1::Version getKernelVersion() const noexcept final;
        [[nodiscard]] tl::expected<IOUringBuf, v1::IOError> createProvidedBuffer(unsigned short entries, unsigned int entryBufferSize) noexcept final;

    private:
        bool checkRingFlags(io_uring* ring);
        void shouldAddQuitEvent();

#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
        void beforeSubmitDebug(unsigned int space);
        void afterSubmitDebug(int ret, unsigned int space);
#endif

        std::unique_ptr<io_uring> _eventQueue{};
        io_uring *_eventQueuePtr{};
        std::atomic<bool> _quit{false};
        std::atomic<bool> _initializedQueue{false};
        std::thread::id _threadId{};
        uint64_t _quitTimeoutMs{};
        unsigned int _entriesCount{};
        std::atomic<uint64_t> _pendingEvents{0};
        int _uringBufIdCounter{1};
        long long _pollTimeoutNs{};
        std::chrono::steady_clock::time_point _whenQuitEventWasSent{};
        std::atomic<bool> _quitEventSent{false};
        v1::Version _kernelVersion{};
        long int _pageSize{};
#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
        std::vector<std::pair<io_uring_op, Ichor::Event*>> _debugOpcodes;
#endif
    };
}
