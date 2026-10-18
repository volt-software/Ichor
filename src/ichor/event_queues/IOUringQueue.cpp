#ifndef ICHOR_USE_LIBURING
#error "Uring not enabled."
#endif

#include <ichor/event_queues/IOUringQueue.h>
#include <ichor/ScopeGuard.h>
#include <ichor/DependencyManager.h>
#include <ichor/dependency_management/InternalServiceLifecycleManager.h>

#include <fmt/core.h>
#include <bit>
#include <limits>
#include <unistd.h>
#include <sys/mman.h>
#include <cstdlib>
#include <algorithm>
#include <memory>
#include <mutex>
#include <utility>
#include <ichor/event_queues/IIOUringQueue.h>
#include <ichor/stl/LinuxUtils.h>
#include <ichor/stl/CompilerSpecific.h>

#if defined(__SANITIZE_THREAD__)
#define TSAN_ENABLED
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define TSAN_ENABLED
#endif
#endif

#ifdef TSAN_ENABLED
#define TSAN_ANNOTATE_HAPPENS_BEFORE(addr) \
    AnnotateHappensBefore(__FILE__, __LINE__, (void*)(addr))
#define TSAN_ANNOTATE_HAPPENS_AFTER(addr) \
    AnnotateHappensAfter(__FILE__, __LINE__, (void*)(addr))
extern "C" void AnnotateHappensBefore(const char* f, int l, void* addr);
extern "C" void AnnotateHappensAfter(const char* f, int l, void* addr);
#else
#define TSAN_ANNOTATE_HAPPENS_BEFORE(addr)
#define TSAN_ANNOTATE_HAPPENS_AFTER(addr)
#endif

#ifdef CONFIG_USE_SANITIZER
#include <sanitizer/asan_interface.h>
#endif

namespace Ichor::v1::Detail {
    extern std::atomic<bool> sigintQuit;
    extern std::atomic<bool> registeredSignalHandler;
    void on_sigint([[maybe_unused]] int sig);
}

template<>
struct fmt::formatter<io_uring_op> {
    constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
        return ctx.end();
    }

    template<typename FormatContext>
    auto format(const io_uring_op &input, FormatContext &ctx) const -> decltype(ctx.out()) {
        switch(input) {
            case io_uring_op::IORING_OP_NOP:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_NOP");
            case io_uring_op::IORING_OP_READV:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_READV");
            case io_uring_op::IORING_OP_WRITEV:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_WRITEV");
            case io_uring_op::IORING_OP_FSYNC:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_FSYNC");
            case io_uring_op::IORING_OP_READ_FIXED:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_READ_FIXED");
            case io_uring_op::IORING_OP_WRITE_FIXED:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_WRITE_FIXED");
            case io_uring_op::IORING_OP_POLL_ADD:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_POLL_ADD");
            case io_uring_op::IORING_OP_POLL_REMOVE:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_POLL_REMOVE");
            case io_uring_op::IORING_OP_SYNC_FILE_RANGE:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_SYNC_FILE_RANGE");
            case io_uring_op::IORING_OP_SENDMSG:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_SENDMSG");
            case io_uring_op::IORING_OP_RECVMSG:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_RECVMSG");
            case io_uring_op::IORING_OP_TIMEOUT:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_TIMEOUT");
            case io_uring_op::IORING_OP_TIMEOUT_REMOVE:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_TIMEOUT_REMOVE");
            case io_uring_op::IORING_OP_ACCEPT:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_ACCEPT");
            case io_uring_op::IORING_OP_ASYNC_CANCEL:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_ASYNC_CANCEL");
            case io_uring_op::IORING_OP_LINK_TIMEOUT:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_LINK_TIMEOUT");
            case io_uring_op::IORING_OP_CONNECT:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_CONNECT");
            case io_uring_op::IORING_OP_FALLOCATE:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_FALLOCATE");
            case io_uring_op::IORING_OP_OPENAT:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_OPENAT");
            case io_uring_op::IORING_OP_CLOSE:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_CLOSE");
            case io_uring_op::IORING_OP_FILES_UPDATE:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_FILES_UPDATE");
            case io_uring_op::IORING_OP_STATX:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_STATX");
            case io_uring_op::IORING_OP_READ:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_READ");
            case io_uring_op::IORING_OP_WRITE:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_WRITE");
            case io_uring_op::IORING_OP_FADVISE:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_FADVISE");
            case io_uring_op::IORING_OP_MADVISE:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_MADVISE");
            case io_uring_op::IORING_OP_SEND:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_SEND");
            case io_uring_op::IORING_OP_RECV:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_RECV");
            case io_uring_op::IORING_OP_OPENAT2:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_OPENAT2");
            case io_uring_op::IORING_OP_EPOLL_CTL:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_EPOLL_CTL");
            case io_uring_op::IORING_OP_SPLICE:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_SPLICE");
            case io_uring_op::IORING_OP_PROVIDE_BUFFERS:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_PROVIDE_BUFFERS");
            case io_uring_op::IORING_OP_REMOVE_BUFFERS:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_REMOVE_BUFFERS");
            case io_uring_op::IORING_OP_TEE:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_TEE");
            case io_uring_op::IORING_OP_SHUTDOWN:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_SHUTDOWN");
            case io_uring_op::IORING_OP_RENAMEAT:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_RENAMEAT");
            case io_uring_op::IORING_OP_UNLINKAT:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_UNLINKAT");
            case io_uring_op::IORING_OP_MKDIRAT:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_MKDIRAT");
            case io_uring_op::IORING_OP_SYMLINKAT:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_SYMLINKAT");
            case io_uring_op::IORING_OP_LINKAT:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_LINKAT");
            case io_uring_op::IORING_OP_MSG_RING:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_MSG_RING");
            case io_uring_op::IORING_OP_FSETXATTR:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_FSETXATTR");
            case io_uring_op::IORING_OP_SETXATTR:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_SETXATTR");
            case io_uring_op::IORING_OP_FGETXATTR:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_FGETXATTR");
            case io_uring_op::IORING_OP_GETXATTR:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_GETXATTR");
            case io_uring_op::IORING_OP_SOCKET:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_SOCKET");
            case io_uring_op::IORING_OP_URING_CMD:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_URING_CMD");
            case io_uring_op::IORING_OP_SEND_ZC:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_SEND_ZC");
            case io_uring_op::IORING_OP_SENDMSG_ZC:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_SENDMSG_ZC");
            case io_uring_op::IORING_OP_READ_MULTISHOT:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_READ_MULTISHOT");
            case io_uring_op::IORING_OP_WAITID:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_WAITID");
            case io_uring_op::IORING_OP_FUTEX_WAIT:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_FUTEX_WAIT");
            case io_uring_op::IORING_OP_FUTEX_WAKE:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_FUTEX_WAKE");
            case io_uring_op::IORING_OP_FUTEX_WAITV:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_FUTEX_WAITV");
            case io_uring_op::IORING_OP_FIXED_FD_INSTALL:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_FIXED_FD_INSTALL");
            case io_uring_op::IORING_OP_FTRUNCATE:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_FTRUNCATE");
            case io_uring_op::IORING_OP_BIND:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_BIND");
            case io_uring_op::IORING_OP_LISTEN:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_LISTEN");
            case io_uring_op::IORING_OP_RECV_ZC:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_RECV_ZC");
            case io_uring_op::IORING_OP_EPOLL_WAIT:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_EPOLL_WAIT");
            case io_uring_op::IORING_OP_READV_FIXED:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_READV_FIXED");
            case io_uring_op::IORING_OP_WRITEV_FIXED:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_WRITEV_FIXED");
            case io_uring_op::IORING_OP_PIPE:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_PIPE");
            case io_uring_op::IORING_OP_NOP128:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_NOP128");
            case io_uring_op::IORING_OP_URING_CMD128:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_URING_CMD128");
            case io_uring_op::IORING_OP_LAST:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_LAST");
        }
        return fmt::format_to(ctx.out(), "io_uring_op:: {} ???  report bug!", static_cast<uint32_t>(input));
    }
};

#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
static inline std::vector<std::pair<io_uring_op, Ichor::Event*>> getToBeSubmittedOpcodes(io_uring *ring) {
    std::vector<std::pair<io_uring_op, Ichor::Event*>> ret;
    struct io_uring_sq *sq = &ring->sq;
    int shift = 0;
    unsigned int head;
    unsigned int tail = sq->sqe_tail;
    if (!(ring->flags & IORING_SETUP_SQPOLL))
        head = *sq->khead;
    else
        head = io_uring_smp_load_acquire(sq->khead);

    if (ring->flags & IORING_SETUP_SQE128) {
        shift = 1;
    }
    ret.reserve(tail - head);

    struct io_uring_sqe *sqe;
    while(head != tail) {
        sqe = &sq->sqes[(head & sq->ring_mask) << shift];
        ret.emplace_back((io_uring_op)sqe->opcode, reinterpret_cast<Ichor::Event*>(sqe->user_data));
        head++;
    }
    return ret;
}
#endif

static int io_uring_ring_dontdump(struct io_uring *ring)
{
    size_t len;
    int ret;

    if (!ring->sq.ring_ptr || !ring->sq.sqes || !ring->cq.ring_ptr)
        return -EINVAL;

    len = sizeof(struct io_uring_sqe);
    if (ring->flags & IORING_SETUP_SQE128)
        len += 64;
    len *= ring->sq.ring_entries;
    ret = madvise(ring->sq.sqes, len, MADV_DONTDUMP);
    if (ret < 0)
        return ret;

    len = ring->sq.ring_sz;
    ret = madvise(ring->sq.ring_ptr, len, MADV_DONTDUMP);
    if (ret < 0)
        return ret;

    if (ring->cq.ring_ptr != ring->sq.ring_ptr) {
        len = ring->cq.ring_sz;
        ret = madvise(ring->cq.ring_ptr, len, MADV_DONTDUMP);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static int io_uring_ring_mlock(struct io_uring *ring)
{
    size_t len;
    int ret;

    if (!ring->sq.ring_ptr || !ring->sq.sqes || !ring->cq.ring_ptr)
        return -EINVAL;

    len = sizeof(struct io_uring_sqe);
    if (ring->flags & IORING_SETUP_SQE128)
        len += 64;
    len *= ring->sq.ring_entries;
    ret = mlock(ring->sq.sqes, len);
    if (ret < 0)
        return ret;

    len = ring->sq.ring_sz;
    ret = mlock(ring->sq.ring_ptr, len);
    if (ret < 0)
        return ret;

    if (ring->cq.ring_ptr != ring->sq.ring_ptr) {
        len = ring->cq.ring_sz;
        ret = mlock(ring->cq.ring_ptr, len);
        if (ret < 0)
            return ret;
    }

    return 0;
}

namespace Ichor {
    IOUringBuf::IOUringBuf(io_uring *eventQueue, io_uring_buf_ring *bufRing, char *entriesBuf, unsigned short entries, unsigned int entryBufferSize, ProvidedBufferIdType id) : _eventQueue(eventQueue), _bufRing(bufRing), _entriesBuf(entriesBuf), _entries(entries), _entryBufferSize(entryBufferSize), _bgid(id), _mask(io_uring_buf_ring_mask(entries)) {
    }

    IOUringBuf::IOUringBuf(IOUringBuf &&o) noexcept : _eventQueue(o._eventQueue), _bufRing(o._bufRing), _entriesBuf(o._entriesBuf), _entries(o._entries), _entryBufferSize(o._entryBufferSize), _bgid(o._bgid), _mask(o._mask) {
        o._eventQueue = nullptr;
    }

    IOUringBuf &IOUringBuf::operator=(IOUringBuf &&o) noexcept {
        _eventQueue = o._eventQueue;
        _bufRing = o._bufRing;
        _entriesBuf = o._entriesBuf;
        _entries = o._entries;
        _entryBufferSize = o._entryBufferSize;
        _bgid = o._bgid;
        _mask = o._mask;
        o._eventQueue = nullptr;
        return *this;
    }

    IOUringBuf::~IOUringBuf() {
        if(_eventQueue != nullptr) {
            io_uring_free_buf_ring(_eventQueue, _bufRing, _entries, _bgid);
            free(_entriesBuf);
        }
    }

    std::string_view IOUringBuf::readMemory(unsigned int entry) const noexcept {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        if(entry >= _entries) {
            std::terminate();
        }
#endif

        return std::string_view{_entriesBuf + static_cast<size_t>(_entryBufferSize) * entry, _entryBufferSize};
    }

    void IOUringBuf::markEntryAvailableAgain(unsigned short entry) noexcept {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        if(entry >= _entries) {
            std::terminate();
        }
#endif
        io_uring_buf_ring_add(_bufRing, _entriesBuf + static_cast<size_t>(_entryBufferSize) * entry, _entryBufferSize, entry, _mask, entry);
        io_uring_buf_ring_advance(_bufRing, 1);
    }
}

namespace Ichor::v1::Detail {
    namespace {
        constexpr unsigned int CROSS_THREAD_RING_ENTRIES = 64;
        // user_data of the IORING_OP_MSG_RING completions that wake up a queue for events pushed from other threads. Never a valid Event*.
        constexpr uint64_t CROSS_THREAD_WAKEUP_DATA = 1;
        constexpr unsigned int CQ_ENTRIES_MULTIPLIER = 4;
        constexpr unsigned int MAX_CQ_ENTRIES = 65536; // IORING_MAX_CQ_ENTRIES

        /// io_uring used to send events with IORING_OP_MSG_RING to queues running on other threads, when the sending thread does not run an IOUringQueue itself.
        /// Created once per thread instead of once per event.
        struct CrossThreadRing final {
            CrossThreadRing() = default;
            CrossThreadRing(CrossThreadRing const &) = delete;
            CrossThreadRing& operator=(CrossThreadRing const &) = delete;
            ~CrossThreadRing() {
                if(initialized) {
                    io_uring_queue_exit(&ring);
                }
            }

            io_uring ring{};
            bool initialized{};
        };

        thread_local CrossThreadRing crossThreadRing{};

        CrossThreadRing &getCrossThreadRing(v1::Version const &kernelVersion) {
            auto &r = crossThreadRing;
            if(r.initialized) [[likely]] {
                return r;
            }

            io_uring_params p{};
            if(kernelVersion >= v1::Version{.major=5, .minor=19, .patch=0}) {
                p.flags |= IORING_SETUP_COOP_TASKRUN;
            }
            if(kernelVersion >= v1::Version{.major=6, .minor=0, .patch=0}) {
                p.flags |= IORING_SETUP_SINGLE_ISSUER;
            }
            auto ret = io_uring_queue_init_params(CROSS_THREAD_RING_ENTRIES, &r.ring, &p);
            if(ret == -ENOSYS) [[unlikely]] {
                fmt::println("io_uring_queue_init_params() for cross thread ring failed, ret {}, probably not enabled in the kernel.", -ret);
                std::terminate();
            }
            if(ret < 0) [[unlikely]] {
                p = {};
                ret = io_uring_queue_init_params(CROSS_THREAD_RING_ENTRIES, &r.ring, &p);
                if(ret < 0) [[unlikely]] {
                    fmt::println("io_uring_queue_init_params() for cross thread ring failed, ret {}", -ret);
                    std::terminate();
                }
            }
            r.initialized = true;
            return r;
        }
    }

    IOUringQueueBase::IOUringQueueBase(std::span<UringResponseEvent> responseEvents, uint64_t quitTimeoutMs, long long pollTimeoutNs, tl::optional<v1::Version> emulateKernelVersion) : _quitTimeoutMs(quitTimeoutMs), _entriesCount(static_cast<unsigned int>(responseEvents.size())), _pollTimeoutNs(pollTimeoutNs) {
        for(auto &evt : responseEvents) {
            releaseResponseEvent(&evt);
        }

        if(io_uring_major_version() != 2 || io_uring_minor_version() != 14) {
            fmt::println("io_uring version is {}.{}, but expected 2.14. Ichor is not compiled correctly.", io_uring_major_version(), io_uring_minor_version());
            std::terminate();
        }
        _threadId = std::this_thread::get_id(); // re-set in functions below, because adding events when the queue isn't running yet cannot be done from another thread.

        auto version = v1::kernelVersion();

        if(!version) {
            std::terminate();
        }

        if(*version < v1::Version{.major=5, .minor=4, .patch=0}) {
            fmt::println("Kernel version {} is too old, use at least 5.4.0 or newer.", *version);
            std::terminate();
        }

        if(*version < v1::Version{.major=5, .minor=18, .patch=0}) {
            fmt::println("WARNING! Kernel version {} is too old to support inserting events from other threads, use at least 5.18.0 or newer.", *version);
        }

        if(emulateKernelVersion && *version < emulateKernelVersion) {
            fmt::println("Kernel version {} is too old to emulate given version {}", *version, *emulateKernelVersion);
            std::terminate();
        }

        if(emulateKernelVersion) {
            _kernelVersion = *emulateKernelVersion;
        } else {
            _kernelVersion = *version;
        }

        _pageSize = sysconf(_SC_PAGESIZE);
        if (_pageSize < 0) {
            fmt::println("Couldn't get page size");
            std::terminate();
        }
    }

    IOUringQueueBase::~IOUringQueueBase() {
        if(_initializedQueue.load(std::memory_order_acquire)) {
            stopDm();
            TSAN_ANNOTATE_HAPPENS_AFTER(_eventQueuePtr);
            if(_eventQueue) {
                io_uring_queue_exit(_eventQueue.get());
            }
        }

        if(v1::Detail::registeredSignalHandler) {
            if(::signal(SIGINT, SIG_DFL) == SIG_ERR) {
//                fmt::println("Couldn't unset signal handler");
            }
        }
    }

    void IOUringQueueBase::pushEventInternal(uint64_t priority, std::unique_ptr<Event> &&event) {
        if(!_initializedQueue.load(std::memory_order_acquire)) [[unlikely]] {
            fmt::println("IOUringQueue not initialized. Call createEventLoop or useEventLoop first.");
            std::terminate();
        }

        if(!event) [[unlikely]] {
            fmt::println("Pushing nullptr");
            std::terminate();
        }

        if(shouldQuit()) {
            //fmt::println("pushEventInternal, should quit! not inserting {}", event->get_name());
            return;
        }

        // TODO hardening
//#ifdef ICHOR_USE_HARDENING
//            if(originatingServiceId != 0 && _services.find(originatingServiceId) == _services.end()) [[unlikely]] {
//                std::terminate();
//            }
//#endif


        _pendingEvents.fetch_add(1, std::memory_order_acq_rel);
        Event *procEvent = event.release();
//        fmt::println("pushEventInternal {} {}", procEvent->get_name(), reinterpret_cast<void*>(procEvent));
        if(std::this_thread::get_id() != _threadId) [[unlikely]] {
            if(_kernelVersion < v1::Version{.major=5, .minor=18, .patch=0}) [[unlikely]] {
                fmt::println("Ignored previous warning about kernel too old for inserting events from other threads. Terminating.");
                std::terminate();
            }

            TSAN_ANNOTATE_HAPPENS_BEFORE(procEvent);
            {
                std::scoped_lock const l{_crossThreadEventsMutex};
                _crossThreadEvents.push_back(procEvent);
            }

            if(_crossThreadWakeupPending.exchange(true, std::memory_order_acq_rel)) {
                // a wake-up for the events pushed so far is already on its way
                return;
            }

            if(HasThreadLocalManager()) {
                auto &q = GetThreadLocalEventQueue();
                if(q.get_queue_name_hash() == get_queue_name_hash()) {
                    // From another IOUringQueue's thread: send the wake-up through its own ring. Not waiting for the completion,
                    // two queue threads waiting on messages to each other would deadlock.
                    auto &ioq = static_cast<IOUringQueueBase&>(q);
                    auto sqe = ioq.getSqe();
                    io_uring_prep_msg_ring(sqe, _eventQueuePtr->ring_fd, 0, CROSS_THREAD_WAKEUP_DATA, 0);
                    return;
                }
            }

            auto &tempQueue = getCrossThreadRing(_kernelVersion);

            // Every push waits for its own completion, so the ring is always empty here.
            auto *sqe = io_uring_get_sqe(&tempQueue.ring);
            if(sqe == nullptr) [[unlikely]] {
                fmt::println("Couldn't get sqe for cross thread ring");
                std::terminate();
            }
            io_uring_prep_msg_ring(sqe, _eventQueuePtr->ring_fd, 0, CROSS_THREAD_WAKEUP_DATA, 0);
            io_uring_sqe_set_data64(sqe, 0);
            TSAN_ANNOTATE_HAPPENS_BEFORE(_eventQueuePtr);
            // Wait for the completion, which gives back-pressure and reports errors on the push that caused them.
            auto ret = io_uring_submit_and_wait(&tempQueue.ring, 1);
            if(ret != 1) [[unlikely]] {
                fmt::println("io_uring_submit_and_wait() for cross thread ring {}", ret);
                fmt::println("submit wrong amount, would result in not waking up the queue");
                std::terminate();
            }

            io_uring_cqe *cqe{};
            do {
                ret = io_uring_wait_cqe(&tempQueue.ring, &cqe);
            } while(ret == -EINTR);
            if(ret < 0) [[unlikely]] {
                fmt::println("io_uring_wait_cqe() for cross thread ring {}", ret);
                fmt::println("couldn't get completion event, would result in not waking up the queue");
                std::terminate();
            }
            if(cqe->res < 0) [[unlikely]] {
                fmt::println("cross thread msg_ring completion event failure {}, would result in not waking up the queue", cqe->res);
                std::terminate();
            }
            io_uring_cqe_seen(&tempQueue.ring, cqe);
        } else {
            auto *sqe = io_uring_get_sqe(_eventQueuePtr);

            if(sqe == nullptr) {
                auto ret = io_uring_submit_and_wait(_eventQueuePtr, 2);

                if(ret < 0) {
                    auto space = io_uring_sq_space_left(_eventQueuePtr);
                    fmt::println("pushEventInternal() error waiting for available slots in ring {} {} {}", ret, _entriesCount, space);
                    fmt::println("io_uring_submit_and_wait() failed, ret {}", -ret);
                    std::terminate();
                }

                sqe = io_uring_get_sqe(_eventQueuePtr);
                if(sqe == nullptr) {
                    auto space = io_uring_sq_space_left(_eventQueuePtr);
                    fmt::println("pushEventInternal() couldn't wait for available slots in ring {} {}", _entriesCount, space);
                    std::terminate();
                }
            }

            io_uring_prep_nop(sqe);
            io_uring_sqe_set_data(sqe, procEvent);

            submitIfNeeded();
        }
    }

    bool IOUringQueueBase::empty() const {
        if(!_initializedQueue.load(std::memory_order_acquire)) [[unlikely]] {
            fmt::println("IOUringQueue not initialized. Call createEventLoop or useEventLoop first.");
            std::terminate();
        }

        return _pendingEvents.load(std::memory_order_acquire) == 0;
    }

    uint64_t IOUringQueueBase::size() const {
        if(!_initializedQueue.load(std::memory_order_acquire)) [[unlikely]] {
            fmt::println("IOUringQueue not initialized. Call createEventLoop or useEventLoop first.");
            std::terminate();
        }

        return _pendingEvents.load(std::memory_order_acquire);
    }

    bool IOUringQueueBase::is_running() const noexcept {
        return !_quit.load(std::memory_order_acquire);
    }

    ICHOR_CONST_FUNC_ATTR NameHashType IOUringQueueBase::get_queue_name_hash() const noexcept {
        return typeNameHash<IOUringQueueBase>();
    }

    tl::optional<v1::NeverNull<io_uring*>> IOUringQueueBase::createEventLoop() {
        if(_entriesCount < 32) {
            fmt::println("entriesCount < 32, expect reduced queue performance.");
        }

        _eventQueue = std::make_unique<io_uring>();
        _eventQueuePtr = _eventQueue.get();
        io_uring_params p{};
        if(_kernelVersion >= v1::Version{.major=5, .minor=19, .patch=0}) {
            p.flags |= IORING_SETUP_COOP_TASKRUN;
        } else {
            fmt::println("IORING_SETUP_COOP_TASKRUN not supported, requires kernel >= 5.19.0, expect reduced performance.");
        }
        if(_kernelVersion >= v1::Version{.major=6, .minor=0, .patch=0}) {
            p.flags |= IORING_SETUP_SINGLE_ISSUER;
        } else {
            fmt::println("IORING_SETUP_SINGLE_ISSUER not supported, requires kernel >= 6.0.0, expect reduced performance.");
        }
        if(_kernelVersion >= v1::Version{.major=6, .minor=1, .patch=0}) {
            p.flags |= IORING_SETUP_DEFER_TASKRUN;
        } else {
            fmt::println("IORING_SETUP_DEFER_TASKRUN not supported, requires kernel >= 6.1.0, expect reduced performance.");
        }
        if(_kernelVersion >= v1::Version{.major=5, .minor=5, .patch=0}) {
            // A larger completion queue avoids the (slow) kernel overflow list when bursts of completions exceed the default of 2 * _entriesCount
            p.flags |= IORING_SETUP_CQSIZE;
            p.cq_entries = std::min(_entriesCount * CQ_ENTRIES_MULTIPLIER, MAX_CQ_ENTRIES);
        }
        p.sq_thread_idle = 50;
        auto ret = io_uring_queue_init_params(_entriesCount, _eventQueuePtr, &p);
        if(ret == -ENOSYS) [[unlikely]] {
            fmt::println("io_uring_queue_init_params() failed, ret {}, probably not enabled in the kernel.", -ret);
            std::terminate();
        }
        if(ret < 0) [[unlikely]] {
            if(p.flags == 0) { // no use in retrying
                fmt::println("io_uring_queue_init_params() failed, ret {}", -ret);
                std::terminate();
            }
            fmt::println("Couldn't set queue params 0x{:X}, retrying without. Expect reduced performance.", p.flags);
            p.flags = 0;
            ret = io_uring_queue_init_params(_entriesCount, _eventQueuePtr, &p);
            if(ret < 0) [[unlikely]] {
                fmt::println("io_uring_queue_init_params() failed, ret {}", -ret);
                std::terminate();
            }
        }
        if(!checkRingFlags(_eventQueuePtr)) {
            return {};
        }
        ret = io_uring_register_ring_fd(_eventQueuePtr);
        if(ret < 0) [[unlikely]] {
            fmt::println("Couldn't register ring fd. Expect reduced performance. Requires kernel >= 5.18 {}", ret);
        }
        ret = io_uring_ring_dontfork(_eventQueuePtr);
        if(ret < 0) [[unlikely]] {
            fmt::println("io_uring_ring_dontfork() failed, ret {}", -ret);
            std::terminate();
        }
        ret = io_uring_ring_dontdump(_eventQueuePtr);
        if(ret < 0) [[unlikely]] {
            fmt::println("io_uring_ring_dontdump() failed, ret {}", -ret);
            std::terminate();
        }
        ret = io_uring_ring_mlock(_eventQueuePtr);
        if(ret < 0) [[unlikely]] {
            fmt::println("warning: io_uring_ring_mlock() failed: {}", errno);
            std::terminate();
        }

        _threadId = std::this_thread::get_id();
        _initializedQueue.store(true, std::memory_order_release);
        return _eventQueuePtr;
    }

    bool IOUringQueueBase::useEventLoop(v1::NeverNull<io_uring*> event) {
        if(event->sq.ring_entries != _entriesCount) {
            fmt::println("useEventLoop() ring has {} entries, but queue expects {}.", event->sq.ring_entries, _entriesCount);
            return false;
        }
        if(event->flags & IORING_SETUP_SQPOLL) {
            // The kernel thread consumes sqes asynchronously, while the queue assumes they are consumed when submitting.
            fmt::println("useEventLoop() rings with IORING_SETUP_SQPOLL are not supported.");
            return false;
        }
        if(!checkRingFlags(event)) {
            return false;
        }
        _threadId = std::this_thread::get_id();
        _eventQueuePtr = event;
        _initializedQueue.store(true, std::memory_order_release);
        return true;
    }

    bool IOUringQueueBase::start(bool captureSigInt) {
        if(!_initializedQueue.load(std::memory_order_acquire)) [[unlikely]] {
            fmt::println("IOUringQueue not initialized. Call createEventLoop or useEventLoop first.");
            return false;
        }

        if(std::this_thread::get_id() != _threadId) [[unlikely]] {
            fmt::println("Creation of ring and start() have to be on the same thread.");
            std::terminate();
        }
//        fmt::println("start() {} {}", std::hash<std::thread::id>{}(std::this_thread::get_id()), std::hash<std::thread::id>{}(_threadId));

        if(captureSigInt && !Ichor::v1::Detail::registeredSignalHandler.exchange(true)) {
            if(::signal(SIGINT, Ichor::v1::Detail::on_sigint) == SIG_ERR) {
                fmt::println("Couldn't set signal");
                return false;
            }
        }

        addInternalServiceManager(std::make_unique<v1::Detail::InternalServiceLifecycleManager<IIOUringQueue>>(this));

        startDm();

        {
            auto space = io_uring_sq_space_left(_eventQueuePtr);
            auto ret = io_uring_submit(_eventQueuePtr);

#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
            beforeSubmitDebug(space);
#endif

            if(ret < 0) [[unlikely]] {
                fmt::println("io_uring_submit() failed, ret {}.", -ret);
                std::terminate();
            }
            if(static_cast<unsigned>(ret) != _entriesCount - space) [[unlikely]] {
//                fmt::println("io_uring_submit {}", ret);
                fmt::println("submit wrong amount1 {} {} {}", ret, _entriesCount, space);
#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
                afterSubmitDebug(ret, space);
#endif
                std::terminate();
            }
        }

        while(!shouldQuit()) [[likely]] {
            io_uring_cqe *cqe{};
            __kernel_timespec ts{};
            ts.tv_nsec = _pollTimeoutNs;

            auto space = io_uring_sq_space_left(_eventQueuePtr);
#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
            if(space < _entriesCount) {
                beforeSubmitDebug(space);
            }
#endif
            // Submit everything queued up while handling the previous batch and wait for new completions in one syscall
            auto ret = io_uring_submit_and_wait_timeout(_eventQueuePtr, &cqe, 1, &ts, nullptr);
//            fmt::println("io_uring_submit_and_wait_timeout {} {}", ret, reinterpret_cast<void*>(cqe));
            if(ret < 0 && ret != -ETIME) [[unlikely]] {
                if(ret == -EBADF) {
                    fmt::println("io_uring_submit_and_wait_timeout() failed, ret {}. Did you create the event loop on the same thread as you started the queue?", -ret);
                } else {
                    fmt::println("io_uring_submit_and_wait_timeout() failed, ret {}", -ret);
                }
                std::terminate();
            }
            // The return value is not always the amount of submitted sqes (e.g. when a cqe was already available), so check that the kernel consumed everything instead.
            if(auto spaceAfter = io_uring_sq_space_left(_eventQueuePtr); spaceAfter != _entriesCount) [[unlikely]] {
                fmt::println("submit wrong amount2 {} {} {} {}", ret, _entriesCount, space, spaceAfter);
#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
                afterSubmitDebug(ret, space);
#endif
                std::terminate();
            }

            unsigned int handled{};
            uint64_t finished{};
            unsigned int head{};
            io_uring_for_each_cqe(_eventQueuePtr, head, cqe) {
                TSAN_ANNOTATE_HAPPENS_AFTER(cqe->user_data);
                if(io_uring_cqe_get_data64(cqe) == CROSS_THREAD_WAKEUP_DATA) {
                    // handled below, after this batch
                    handled++;
                    continue;
                }
                auto *evt = reinterpret_cast<Event *>(io_uring_cqe_get_data(cqe));
                if(evt != nullptr) {
//                    INTERNAL_IO_DEBUG("processing {}", evt->get_name());
                    if(evt->get_type() == UringResponseEvent::TYPE) {
                        auto *resEvt = static_cast<UringResponseEvent*>(evt);
                        resEvt->fun(cqe);
                        if((cqe->flags & IORING_CQE_F_MORE) != IORING_CQE_F_MORE) {
                            releaseResponseEvent(resEvt);
                            finished++;
                        }
//                        else {
//                            fmt::println("F_MORE {}", evt->id);
//                        }
                    } else {
                        std::unique_ptr<Event> uniqueEvt{evt};
                        processEvent(uniqueEvt);
                        // processEvent may take ownership of the event, in which case it is not finished yet
                        if(uniqueEvt) {
                            finished++;
                        }
                    }
                }
                handled++;
            }
            io_uring_cq_advance(_eventQueuePtr, handled);
            // Checked every iteration instead of only on a wake-up completion, so that events can't get stuck if a wake-up got lost.
            if(_crossThreadWakeupPending.load(std::memory_order_acquire)) {
                finished += processCrossThreadEvents();
            }
            if(finished > 0) {
                _pendingEvents.fetch_sub(finished, std::memory_order_acq_rel);
            }

            shouldAddQuitEvent();
        }

        // The loop above only submits at the start of an iteration, so events pushed while handling the last batch are still in the
        // submission queue. Submit them, so that the loop below reaps and de-allocates them.
        if(auto ret = io_uring_submit(_eventQueuePtr); ret < 0) [[unlikely]] {
            fmt::println("io_uring_submit() before final loop failed, ret {}", -ret);
            std::terminate();
        }

        // Cancel everything still in flight (e.g. multishot recvs/accepts that keep completing), otherwise the loop below wouldn't end.
        auto cancelAllInFlight = [this]() {
            if(_kernelVersion < v1::Version{.major=5, .minor=19, .patch=0}) {
                return;
            }
            auto *sqe = getSqe();
            io_uring_prep_cancel64(sqe, 0, IORING_ASYNC_CANCEL_ALL | IORING_ASYNC_CANCEL_ANY);
            if(auto ret = io_uring_submit(_eventQueuePtr); ret < 0) [[unlikely]] {
                fmt::println("io_uring_submit() of cancel before final loop failed, ret {}", -ret);
                std::terminate();
            }
        };
        cancelAllInFlight();

        // one last loop to de-allocate any potentially unhandled events. Bounded, in case requests keep completing anyway (e.g. kernels < 5.19).
        auto const drainDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(_quitTimeoutMs);
//        fmt::println("One last loop");
        bool somethingHandled{true};
        while(somethingHandled && std::chrono::steady_clock::now() < drainDeadline) {
            io_uring_cqe *cqe{};
            __kernel_timespec ts{};
            ts.tv_nsec = 500'000;
            unsigned int handled{};
            unsigned int head{};
            bool stillInFlight{};

            auto ret = io_uring_wait_cqes(_eventQueuePtr, &cqe, 1, &ts, nullptr);
            if(ret < 0 && ret != -ETIME) [[unlikely]] {
                fmt::println("io_uring_wait_cqes() failed, ret {}", -ret);
                std::terminate();
            }
            ret = io_uring_get_events(_eventQueuePtr);
            if(ret < 0) {
                fmt::println("io_uring_get_events() failed, ret {}", -ret);
                std::terminate();
            }
            io_uring_for_each_cqe(_eventQueuePtr, head, cqe) {
                TSAN_ANNOTATE_HAPPENS_AFTER(cqe->user_data);
                if(io_uring_cqe_get_data64(cqe) == CROSS_THREAD_WAKEUP_DATA) {
                    handled++;
                    continue;
                }
                auto *evt = reinterpret_cast<Event *>(io_uring_cqe_get_data(cqe));
//                fmt::println("last loop processing {}", evt->get_name());
                if(evt != nullptr) {
                    if(evt->get_type() == UringResponseEvent::TYPE) {
                        if((cqe->flags & IORING_CQE_F_MORE) != IORING_CQE_F_MORE) {
                            releaseResponseEvent(static_cast<UringResponseEvent*>(evt));
                            _pendingEvents.fetch_sub(1, std::memory_order_acq_rel);
                        } else {
                            stillInFlight = true;
                        }
                    } else {
                        delete evt;
                        _pendingEvents.fetch_sub(1, std::memory_order_acq_rel);
                    }
                }
                handled++;
            }

            if(handled == 0) {
                somethingHandled = false;
            }
            io_uring_cq_advance(_eventQueuePtr, handled);
            if(stillInFlight) {
                // The cancel can miss a multishot request, e.g. a multishot timeout between expiring and being re-armed. It was just re-armed.
                cancelAllInFlight();
            }
        }

        {
            // events pushed from other threads that weren't processed anymore
            std::scoped_lock const l{_crossThreadEventsMutex};
            for(auto *evt : _crossThreadEvents) {
                delete evt;
                _pendingEvents.fetch_sub(1, std::memory_order_acq_rel);
            }
            _crossThreadEvents.clear();
        }

        stopDm();

        return true;
    }

    bool IOUringQueueBase::shouldQuit() {
        if(_quitEventSent.load(std::memory_order_acquire) && std::chrono::steady_clock::now() - _whenQuitEventWasSent >= std::chrono::milliseconds(_quitTimeoutMs)) [[unlikely]] {
            _quit.store(true, std::memory_order_release);
        }

        return _quit.load(std::memory_order_acquire);
    }

    void IOUringQueueBase::quit() {
        _quit.store(true, std::memory_order_release);
    }

    v1::NeverNull<io_uring*> IOUringQueueBase::getRing() noexcept {
        if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
            if(_eventQueuePtr == nullptr) [[unlikely]] {
                std::terminate();
            }
        }
        return _eventQueuePtr;
    }

    unsigned int IOUringQueueBase::getMaxEntriesCount() const noexcept {
        return _entriesCount;
    }

    uint32_t IOUringQueueBase::sqeSpaceLeft() const noexcept {
        return io_uring_sq_space_left(_eventQueuePtr);
    }

    io_uring_sqe *IOUringQueueBase::getSqe() noexcept {
        INTERNAL_IO_DEBUG("getSqe");
        auto sqe = io_uring_get_sqe(_eventQueuePtr);
        if(sqe == nullptr) {
            submitIfNeeded();
            sqe = io_uring_get_sqe(_eventQueuePtr);
            if(sqe == nullptr) {
                fmt::println("Couldn't get sqe from ring after submit.");
                std::terminate();
            }
        }
        sqe->user_data = 0;
        return sqe;
    }

    io_uring_sqe* IOUringQueueBase::getSqeWithData(IService *self, UringCallback fun) noexcept {
        INTERNAL_IO_DEBUG("getSqeWithData");
        auto sqe = getSqe();
        io_uring_sqe_set_data(sqe, acquireResponseEvent(self->getServiceId(), std::move(fun)));
        _pendingEvents.fetch_add(1, std::memory_order_acq_rel);
        return sqe;
    }

    io_uring_sqe* IOUringQueueBase::getSqeWithData(ServiceIdType serviceId, UringCallback fun) noexcept {
        INTERNAL_IO_DEBUG("getSqeWithData");
        auto sqe = getSqe();
        io_uring_sqe_set_data(sqe, acquireResponseEvent(serviceId, std::move(fun)));
        _pendingEvents.fetch_add(1, std::memory_order_acq_rel);
        return sqe;
    }

    void IOUringQueueBase::submitIfNeeded() {
        auto space = io_uring_sq_space_left(_eventQueuePtr);
        if(space == 0) {
//            fmt::println("submit {}", _entriesCount);
#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
            beforeSubmitDebug(space);
#endif
            auto ret = io_uring_submit(_eventQueuePtr);
            if(ret < 0) [[unlikely]] {
                fmt::println("io_uring_submit() failed, ret {}", -ret);
                std::terminate();
            }
            if(std::cmp_not_equal(ret, _entriesCount)) [[unlikely]] {
                fmt::println("submit wrong amount3 {} {} {}", ret, _entriesCount, space);
#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
                afterSubmitDebug(ret, space);
#endif
                std::terminate();
            }
        }
    }

    void IOUringQueueBase::forceSubmit() {
        auto space = io_uring_sq_space_left(_eventQueuePtr);
#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
        beforeSubmitDebug(space);
#endif
        auto ret = io_uring_submit(_eventQueuePtr);
        if(ret < 0) [[unlikely]] {
            fmt::println("io_uring_submit() failed, ret {}", -ret);
            std::terminate();
        }
        if(static_cast<unsigned int>(ret) != _entriesCount - space) [[unlikely]] {
            fmt::println("submit wrong amount4 {} {} {}", ret, _entriesCount, space);
#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
            afterSubmitDebug(ret, space);
#endif
            std::terminate();
        }
    }

    void IOUringQueueBase::submitAndWait(uint32_t waitNr) {
        auto space = io_uring_sq_space_left(_eventQueuePtr);
//        fmt::println("submit and wait {}", toSubmit);
#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
        beforeSubmitDebug(space);
#endif
        auto ret = io_uring_submit_and_wait(_eventQueuePtr, waitNr);

        if(ret < 0) [[unlikely]] {
            fmt::println("pushEventInternal() error waiting for available slots in ring {} {} {}", ret, _entriesCount, space);
            fmt::println("io_uring_submit_and_wait() failed, ret {}", -ret);
            std::terminate();
        }

        if(static_cast<unsigned int>(ret) != _entriesCount - space) [[unlikely]] {
            fmt::println("submit wrong amount5 {} {} {}", ret, _entriesCount, space);
#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
            afterSubmitDebug(ret, space);
#endif
            std::terminate();
        }
    }

    v1::Version IOUringQueueBase::getKernelVersion() const noexcept {
        return _kernelVersion;
    }

    bool IOUringQueueBase::checkRingFlags(io_uring *ring) {
        if(!(ring->features & IORING_FEAT_FAST_POLL)) {
            fmt::println("IORING_FEAT_FAST_POLL not supported, requires kernel >= 5.7.0, expect reduced queue performance.");
        }
        if(!(ring->features & IORING_FEAT_NODROP)) {
            fmt::println("IORING_FEAT_NODROP not supported, requires kernel >= 5.5.0, cqe overflow will lead to dropped packets!");
        }
        if(!(ring->features & IORING_FEAT_EXT_ARG)) {
            fmt::println("IORING_FEAT_EXT_ARG (a.k.a. IORING_ENTER_EXT_ARG), requires kernel >= 5.11.0, not supported, expect reduced queue performance.");
        }
        if(!(ring->features & IORING_FEAT_SUBMIT_STABLE)) {
            fmt::println("IORING_FEAT_SUBMIT_STABLE not supported, requires kernel >= 5.5.0, expect reduced queue performance.");
            // it seems all code in Ichor so far keeps state stable until completion, so return false shouldn't be necessary.
//            return false;
        }
        if(_kernelVersion < v1::Version{.major=6, .minor=7, .patch=0}) {
            fmt::println("SOCKET_URING_OP_GETSOCKOPT/SOCKET_URING_OP_SETSOCKOPT not supported, requires kernel >= 6.7.0, expect reduced socket creation performance.");
        }
        if(_kernelVersion < v1::Version{.major=6, .minor=0, .patch=0}) {
            fmt::println("io_uring_prep_recv_multishot not supported, requires kernel >= 6.0.0, expect reduced socket performance.");
        }
        if(_kernelVersion < v1::Version{.major=5, .minor=19, .patch=0}) {
            fmt::println("io_uring_prep_multishot_accept not supported, requires kernel >= 5.19.0, expect reduced socket performance.");
        }
        return true;
    }

    void IOUringQueueBase::shouldAddQuitEvent() {
        bool const shouldQuit = v1::Detail::sigintQuit.load(std::memory_order_acquire);

        if(shouldQuit && !_quitEventSent.load(std::memory_order_acquire)) {
            pushEventInternal(INTERNAL_EVENT_PRIORITY, std::make_unique<QuitEvent>(getNextEventId(), ServiceIdType{0}, INTERNAL_EVENT_PRIORITY));
            _quitEventSent.store(true, std::memory_order_release);
            _whenQuitEventWasSent = std::chrono::steady_clock::now();
        }
    }

    uint64_t IOUringQueueBase::processCrossThreadEvents() {
        // Cleared before taking the events: a push after this sends a new wake-up, a push before it is taken below.
        _crossThreadWakeupPending.store(false, std::memory_order_release);
        {
            std::scoped_lock const l{_crossThreadEventsMutex};
            std::swap(_crossThreadEvents, _crossThreadEventsProcessing);
        }

        uint64_t finished{};
        for(auto *evt : _crossThreadEventsProcessing) {
            TSAN_ANNOTATE_HAPPENS_AFTER(evt);
            std::unique_ptr<Event> uniqueEvt{evt};
            processEvent(uniqueEvt);
            // processEvent may take ownership of the event, in which case it is not finished yet
            if(uniqueEvt) {
                finished++;
            }
        }
        _crossThreadEventsProcessing.clear();
        return finished;
    }

    UringResponseEvent *IOUringQueueBase::acquireResponseEvent(ServiceIdType serviceId, UringCallback &&fun) noexcept {
        auto *evt = _freeResponseEventsHead;
        if(evt == nullptr) [[unlikely]] {
            // More requests in flight than pooled events (e.g. many open connections each with a pending recv), grow the pool.
            // Pending requests are not limited by the ring size, so this must not be fatal.
            return _overflowResponseEvents.emplace_back(std::make_unique<UringResponseEvent>(getNextEventId(), serviceId, INTERNAL_EVENT_PRIORITY, std::move(fun))).get();
        }
        _freeResponseEventsHead = evt->nextFree;
        if(_freeResponseEventsHead == nullptr) {
            _freeResponseEventsTail = nullptr;
        }
        // Event has const members, so re-construct instead of assigning
        std::destroy_at(evt);
        return std::construct_at(evt, getNextEventId(), serviceId, INTERNAL_EVENT_PRIORITY, std::move(fun));
    }

    void IOUringQueueBase::releaseResponseEvent(UringResponseEvent *evt) noexcept {
        // release resources held by the callback early
        evt->fun.reset();
        evt->nextFree = nullptr;
        if(_freeResponseEventsTail == nullptr) {
            _freeResponseEventsHead = evt;
        } else {
            _freeResponseEventsTail->nextFree = evt;
        }
        _freeResponseEventsTail = evt;
    }

    tl::expected<IOUringBuf, v1::IOError> IOUringQueueBase::createProvidedBuffer(unsigned short entries, unsigned int entryBufferSize) noexcept {
        if(entries == 0) {
            fmt::println("createProvidedBuffer entries == 0.");
            return tl::unexpected(v1::IOError::NOT_SUPPORTED);
        }

        if(entries > 32768) {
            return tl::unexpected(v1::IOError::MESSAGE_SIZE_TOO_BIG);
        }

        if(!std::has_single_bit(entries)) {
            fmt::println("createProvidedBuffer entries not a power of two.");
            return tl::unexpected(v1::IOError::NOT_SUPPORTED);
        }

        // Does not fit in size_t on 32-bit platforms
        if(entryBufferSize > std::numeric_limits<size_t>::max() / entries) {
            return tl::unexpected(v1::IOError::MESSAGE_SIZE_TOO_BIG);
        }
        size_t const entriesBufSize = static_cast<size_t>(entries) * entryBufferSize;

        if(_kernelVersion < v1::Version{.major=5, .minor=19, .patch=0}) {
            return tl::unexpected(v1::IOError::KERNEL_TOO_OLD);
        }

        char *entriesBuf = static_cast<char *>(aligned_alloc(static_cast<size_t>(_pageSize), entriesBufSize));
        if(entriesBuf == nullptr) {
            return tl::unexpected(v1::IOError::NO_MEMORY_AVAILABLE);
        }
        io_uring_buf_ring *bufRing{};
        auto id = _uringBufIdCounter++;
        bool created{};
        ScopeGuard freeIfNotCreated{[&]() noexcept {
            if(created) {
                return;
            }
            if(bufRing != nullptr) {
                io_uring_free_buf_ring(_eventQueuePtr, bufRing, entries, id);
            }
            free(entriesBuf);
        }};
        if(madvise(entriesBuf, entriesBufSize, MADV_DONTDUMP) != 0) {
            fmt::println("createProvidedBuffer madvise entriesBuf MADV_DONTDUMP failed.");
            return tl::unexpected(v1::IOError::NOT_SUPPORTED);
        }
        if(madvise(entriesBuf, entriesBufSize, MADV_DONTFORK) != 0) {
            fmt::println("createProvidedBuffer madvise entriesBuf MADV_DONTFORK failed.");
            return tl::unexpected(v1::IOError::NOT_SUPPORTED);
        }

        int ret;
        bufRing = io_uring_setup_buf_ring(_eventQueuePtr, entries, id, 0, &ret);
        if(bufRing == nullptr) {
            return tl::unexpected(v1::mapErrnoToError(-ret));
        }
        if(madvise(bufRing, entries * sizeof(struct io_uring_buf), MADV_DONTDUMP) != 0) {
            fmt::println("createProvidedBuffer madvise bufRing MADV_DONTDUMP failed.");
            return tl::unexpected(v1::IOError::NOT_SUPPORTED);
        }
        if(madvise(bufRing, entries * sizeof(struct io_uring_buf), MADV_DONTFORK) != 0) {
            fmt::println("createProvidedBuffer madvise bufRing MADV_DONTFORK failed.");
            return tl::unexpected(v1::IOError::NOT_SUPPORTED);
        }

        char *ptr = entriesBuf;
        auto mask = io_uring_buf_ring_mask(entries);
        for (unsigned short i = 0; i < entries; i++) {
            io_uring_buf_ring_add(bufRing, ptr, entryBufferSize, i, mask, i);
            ptr += entryBufferSize;
        }
        io_uring_buf_ring_advance(bufRing, entries);

        created = true;
        return IOUringBuf(_eventQueuePtr, bufRing, entriesBuf, entries, entryBufferSize, id);
    }
}

#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
    void Ichor::v1::Detail::IOUringQueueBase::beforeSubmitDebug(unsigned int space) {
        fmt::println("going to submit {} {}", _entriesCount, space);
        _debugOpcodes = getToBeSubmittedOpcodes(_eventQueuePtr);
        for(auto &op : _debugOpcodes) {
            auto svc = _dm->getIService(op.second->originatingService);
            std::string_view svcName = "UNKNOWN";
            if(svc) {
                svcName = (*svc)->getServiceName();
            }
            fmt::println("{} from {}:{} with event type {}", op.first, op.second->originatingService, svcName, op.second->get_name());
        }
    }

    void Ichor::v1::Detail::IOUringQueueBase::afterSubmitDebug(int ret, unsigned int space) {
        fmt::println("still left in queue {} {} {}", ret, _entriesCount, space);
        auto newSubmitLog = getToBeSubmittedOpcodes(_eventQueuePtr);
        for(auto &op : newSubmitLog) {
            auto svc = _dm->getIService(op.second->originatingService);
            std::string_view svcName = "UNKNOWN";
            if(svc) {
                svcName = (*svc)->getServiceName();
            }
            fmt::println("{} from {}:{} with event type {}", op.first, op.second->originatingService, svcName, op.second->get_name());
        }
        fmt::println("");
        auto diff = _debugOpcodes.size() - newSubmitLog.size();
        for(decltype(diff) i = 0; i < diff; i++) {
            fmt::println("submitted {}", _debugOpcodes[i].first);
        }
        fmt::println("");
        fmt::println("The last submission probably contains an error.");
    }
#endif
