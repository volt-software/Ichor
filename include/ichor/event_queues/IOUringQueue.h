#pragma once

#ifndef ICHOR_USE_LIBURING
#error "Ichor has not been compiled with io_uring support"
#endif

#include <ichor/event_queues/IIOUringQueue.h>
#include <ichor/ichor_liburing.h>
#include <ichor/ConstevalHash.h>
#include <array>
#include <atomic>
#include <span>
#include <thread>
#include <chrono>

namespace Ichor {
    namespace Detail {
        /// Event used as user_data for sqe's requested through IIOUringQueue::getSqeWithData. Instances are pooled by TemplateIOUringQueue and never heap allocated.
        struct UringResponseEvent final : public Event {
            UringResponseEvent() noexcept : Event(0, ServiceIdType{0}, INTERNAL_EVENT_PRIORITY) {}
            UringResponseEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority, UringCallback _fun) noexcept :
                    Event(_id, _originatingService, _priority), fun(std::move(_fun)) {}
            ~UringResponseEvent() final = default;

            [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
                return NAME;
            }
            [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
                return TYPE;
            }

            UringCallback fun;
            UringResponseEvent *nextFree{};
            static constexpr NameHashType TYPE = typeNameHash<UringResponseEvent>();
            static constexpr std::string_view NAME = typeName<UringResponseEvent>();
        };

        template <uint32_t RingSize>
        struct UringResponseEventStorage {
            std::array<UringResponseEvent, RingSize> _responseEvents{};
        };

        /// Non-templated implementation of TemplateIOUringQueue, use TemplateIOUringQueue or IOUringQueue instead.
        class IOUringQueueBase : public IIOUringQueue {
        public:
            ~IOUringQueueBase() override;

            void pushEventInternal(uint64_t priority, std::unique_ptr<Event> &&event) final;

            [[nodiscard]] bool empty() const final;
            [[nodiscard]] uint64_t size() const final;
            [[nodiscard]] bool is_running() const noexcept final;
            [[nodiscard]] ICHOR_CONST_FUNC_ATTR NameHashType get_queue_name_hash() const noexcept final;

            /// Creates an io_uring event loop with a standard flagset and the ring size given as template parameter.
            /// \return
            [[nodiscard]] tl::optional<v1::NeverNull<io_uring*>> createEventLoop();
            /// Use given ring.
            /// \param loop ring, has to be created with the ring size given as template parameter
            /// \return false if ring doesn't support features required by Ichor, true otherwise
            bool useEventLoop(v1::NeverNull<io_uring*> loop);

            bool start(bool captureSigInt) final;
            [[nodiscard]] bool shouldQuit() final;
            void quit() final;

            [[nodiscard]] v1::NeverNull<io_uring*> getRing() noexcept final;
            [[nodiscard]] unsigned int getMaxEntriesCount() const noexcept final;

            [[nodiscard]] uint32_t sqeSpaceLeft() const noexcept final;
            [[nodiscard]] io_uring_sqe *getSqe() noexcept final;
            io_uring_sqe* getSqeWithData(IService *self, UringCallback fun) noexcept final;
            io_uring_sqe* getSqeWithData(ServiceIdType serviceId, UringCallback fun) noexcept final;

            void submitIfNeeded() final;
            void forceSubmit() final;
            void submitAndWait(uint32_t waitNr) final;

            [[nodiscard]] v1::Version getKernelVersion() const noexcept final;
            [[nodiscard]] tl::expected<IOUringBuf, v1::IOError> createProvidedBuffer(unsigned short entries, unsigned int entryBufferSize) noexcept final;

        protected:
            IOUringQueueBase(std::span<UringResponseEvent> responseEvents, uint64_t quitTimeoutMs, long long pollTimeoutNs, tl::optional<v1::Version> emulateKernelVersion);

        private:
            bool checkRingFlags(io_uring* ring);
            void shouldAddQuitEvent();
            [[nodiscard]] UringResponseEvent *acquireResponseEvent(ServiceIdType serviceId, UringCallback &&fun) noexcept;
            void releaseResponseEvent(UringResponseEvent *evt) noexcept;

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
            // FIFO free list of response events, to make re-use of the same user_data for different requests as unlikely as possible
            UringResponseEvent *_freeResponseEventsHead{};
            UringResponseEvent *_freeResponseEventsTail{};
#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
            std::vector<std::pair<io_uring_op, Ichor::Event*>> _debugOpcodes;
#endif
        };
    }

    /// Provides an io_uring based queue, expects the running OS to have at least kernel 5.4, but multithreading support is only available from 5.18 and later.
    /// \tparam RingSize amount of entries in the submission queue of the ring. Also the maximum amount of in-flight requests created through getSqeWithData, as the events for these are allocated up-front.
    template <uint32_t RingSize>
    class TemplateIOUringQueue final : private Detail::UringResponseEventStorage<RingSize>, public Detail::IOUringQueueBase {
        static_assert(RingSize > 0, "RingSize cannot be 0");
        static_assert(RingSize <= 32768, "RingSize cannot be larger than 32768");
        static_assert((RingSize & (RingSize - 1)) == 0, "RingSize has to be a power of two");
    public:
        explicit TemplateIOUringQueue(uint64_t quitTimeoutMs = 5'000, long long pollTimeoutNs = 100'000'000, tl::optional<v1::Version> emulatedKernelVersion = {}) :
                IOUringQueueBase(this->_responseEvents, quitTimeoutMs, pollTimeoutNs, emulatedKernelVersion) {}
        ~TemplateIOUringQueue() final = default;
    };

    using IOUringQueue = TemplateIOUringQueue<2048>;
}
