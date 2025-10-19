#ifndef ICHOR_USE_LIBURING
#error "Uring not enabled."
#endif

#include <ichor/event_queues/IOUringQueue.h>
#include <ichor/ScopeGuard.h>
#include <ichor/DependencyManager.h>
#include <ichor/dependency_management/InternalServiceLifecycleManager.h>

#include <fmt/core.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstdlib>
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

namespace Ichor::Detail {
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
            case io_uring_op::IORING_OP_LAST:
                return fmt::format_to(ctx.out(), "io_uring_op::IORING_OP_LAST");
        }
        return fmt::format_to(ctx.out(), "io_uring_op:: {} ???  report bug!", (uint32_t)input);
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
        if(entry > _entries) {
            std::terminate();
        }
#endif

        return std::string_view{_entriesBuf + _entryBufferSize * entry, _entryBufferSize};
    }

    void IOUringBuf::markEntryAvailableAgain(unsigned short entry) noexcept {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        if(entry > _entries) {
            std::terminate();
        }
#endif
        io_uring_buf_ring_add(_bufRing, _entriesBuf + _entryBufferSize * entry, _entryBufferSize, entry, _mask, entry);
        io_uring_buf_ring_advance(_bufRing, 1);
    }

    struct UringResponseEvent final : public Event {
        explicit UringResponseEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, std::function<void(io_uring_cqe*)> _fun) noexcept :
                Event(_id, _originatingService, _priority), fun(std::move(_fun)) {}
        ~UringResponseEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
            return TYPE;
        }

        std::function<void(io_uring_cqe*)> fun;
        int32_t res{};
        static constexpr NameHashType TYPE = typeNameHash<UringResponseEvent>();
        static constexpr std::string_view NAME = typeName<UringResponseEvent>();
    };

    IOUringQueue::IOUringQueue(uint64_t quitTimeoutMs, long long pollTimeoutNs, tl::optional<v1::Version> emulateKernelVersion) : _quitTimeoutMs(quitTimeoutMs), _pollTimeoutNs(pollTimeoutNs) {
        if(io_uring_major_version() != 2 || io_uring_minor_version() != 12) {
            fmt::println("io_uring version is {}.{}, but expected 2.12. Ichor is not compiled correctly.", io_uring_major_version(), io_uring_minor_version());
            std::terminate();
        }
        _threadId = std::this_thread::get_id(); // re-set in functions below, because adding events when the queue isn't running yet cannot be done from another thread.

        auto version = v1::kernelVersion();

        if(!version) {
            std::terminate();
        }

        if(*version < v1::Version{5, 4, 0}) {
            fmt::println("Kernel version {} is too old, use at least 5.4.0 or newer.", *version);
            std::terminate();
        }

        if(*version < v1::Version{5, 18, 0}) {
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

    IOUringQueue::~IOUringQueue() {
        if(_initializedQueue.load(std::memory_order_acquire)) {
            if(_dm) {
                stopDm();
            }
            TSAN_ANNOTATE_HAPPENS_AFTER(_eventQueuePtr);
            if(_eventQueue) {
                io_uring_queue_exit(_eventQueue.get());
            }
        }

        if(Detail::registeredSignalHandler) {
            if(::signal(SIGINT, SIG_DFL) == SIG_ERR) {
//                fmt::println("Couldn't unset signal handler");
            }
        }
    }

    void IOUringQueue::pushEventInternal(uint64_t priority, std::unique_ptr<Event> &&event) {
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
            if(_kernelVersion < v1::Version{5, 18, 0}) [[unlikely]] {
                fmt::println("Ignored previous warning about kernel too old for inserting events from other threads. Terminating.");
                std::terminate();
            }

            TSAN_ANNOTATE_HAPPENS_BEFORE(procEvent);

            if(HasThreadLocalManager()) {
                auto &q = GetThreadLocalEventQueue();
                if(q.get_queue_name_hash() == get_queue_name_hash()) {
                    auto &ioq = static_cast<IOUringQueue&>(q);
                    auto sqe = ioq.getSqe();
                    io_uring_prep_msg_ring(sqe, _eventQueuePtr->ring_fd, 0, reinterpret_cast<uintptr_t>(reinterpret_cast<void*>(procEvent)), 0);
                    return;
                }
            }

            io_uring tempQueue{};
            io_uring_params p{};
            p.flags = IORING_SETUP_ATTACH_WQ;
            if(_kernelVersion >= v1::Version{5, 19, 0}) {
                p.flags |= IORING_SETUP_COOP_TASKRUN;
            } else {
                fmt::println("IORING_SETUP_COOP_TASKRUN not supported, requires kernel >= 5.19.0, expect reduced performance.");
            }
            if(_kernelVersion >= v1::Version{6, 0, 0}) {
                p.flags |= IORING_SETUP_SINGLE_ISSUER;
            } else {
                fmt::println("IORING_SETUP_SINGLE_ISSUER not supported, requires kernel >= 6.0.0, expect reduced performance.");
            }
            if(_kernelVersion >= v1::Version{6, 1, 0}) {
                p.flags |= IORING_SETUP_DEFER_TASKRUN;
            } else {
                fmt::println("IORING_SETUP_DEFER_TASKRUN not supported, requires kernel >= 6.1.0, expect reduced performance.");
            }
            p.wq_fd = static_cast<__u32>(_eventQueuePtr->ring_fd);
            auto ret = io_uring_queue_init_params(4, &tempQueue, &p);
            if(ret == -ENOSYS) [[unlikely]] {
                // TODO replace all throws with prints + terminates
                fmt::println("io_uring_queue_init_params() with WQ failed, ret {}, probably not enabled in the kernel.", -ret);
                std::terminate();
            }
            if(ret < 0) [[unlikely]] {
                if(p.flags == IORING_SETUP_ATTACH_WQ) { // no use in retrying
                    fmt::println("io_uring_queue_init_params() failed, ret {}", -ret);
                    std::terminate();
                }
                fmt::println("Couldn't set queue params 0x{:X}, retrying without. Expect reduced performance.", p.flags);
                p.flags = IORING_SETUP_ATTACH_WQ;
                ret = io_uring_queue_init_params(4, &tempQueue, &p);
                if(ret < 0) [[unlikely]] {
                    fmt::println("io_uring_queue_init_params() for tempQueue failed, ret {}", -ret);
                    std::terminate();
                }
            }
            ScopeGuard sg{[&tempQueue]() {
                io_uring_queue_exit(&tempQueue);
            }};

            auto sqe = io_uring_get_sqe(&tempQueue);
            io_uring_prep_msg_ring(sqe, _eventQueuePtr->ring_fd, 0, reinterpret_cast<uintptr_t>(reinterpret_cast<void*>(procEvent)), 0);
            TSAN_ANNOTATE_HAPPENS_BEFORE(_eventQueuePtr);
            ret = io_uring_submit_and_wait(&tempQueue, 1);
            if(ret != 1) [[unlikely]] {
                fmt::println("io_uring_submit_and_get_events {}", ret);
                fmt::println("submit wrong amount, would result in dropping event");
                std::terminate();
            }

            io_uring_cqe *cqe;
            ret = io_uring_wait_cqe(&tempQueue, &cqe);
            if(ret < 0) {
                fmt::println("io_uring_wait_cqe {}", ret);
                fmt::println("couldn't get completion event, would result in dropping event");
                std::terminate();
            }
            if(cqe->res < 0) {
                fmt::println("completion event {}", cqe->res);
                fmt::println("Completion event failure, would result in dropping event");
                std::terminate();
            }
            io_uring_cqe_seen(&tempQueue, cqe);

//            fmt::println("pushEventInternal() {} {}", std::hash<std::thread::id>{}(std::this_thread::get_id()), std::hash<std::thread::id>{}(_threadId));

//            std::terminate();
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

    bool IOUringQueue::empty() const {
        if(!_initializedQueue.load(std::memory_order_acquire)) [[unlikely]] {
            fmt::println("IOUringQueue not initialized. Call createEventLoop or useEventLoop first.");
            std::terminate();
        }

        return _pendingEvents.load(std::memory_order_acquire) == 0;
    }

    uint64_t IOUringQueue::size() const {
        if(!_initializedQueue.load(std::memory_order_acquire)) [[unlikely]] {
            fmt::println("IOUringQueue not initialized. Call createEventLoop or useEventLoop first.");
            std::terminate();
        }

        return _pendingEvents.load(std::memory_order_acquire);
    }

    bool IOUringQueue::is_running() const noexcept {
        return !_quit.load(std::memory_order_acquire);
    }

    ICHOR_CONST_FUNC_ATTR NameHashType IOUringQueue::get_queue_name_hash() const noexcept {
        return typeNameHash<IOUringQueue>();
    }

    tl::optional<v1::NeverNull<io_uring*>> IOUringQueue::createEventLoop(unsigned entriesCount) {
        if(entriesCount == 0) {
            fmt::println("Cannot create an event loop with 0 entries.");
            return {};
        }
        if(entriesCount < 32) {
            fmt::println("entriesCount < 32, expect reduced queue performance.");
        }

        ICHOR_ASSUME(entriesCount > 0);

        _eventQueue = std::make_unique<io_uring>();
        _eventQueuePtr = _eventQueue.get();
        _entriesCount = entriesCount;
        io_uring_params p{};
        if(_kernelVersion >= v1::Version{5, 19, 0}) {
            p.flags |= IORING_SETUP_COOP_TASKRUN;
        } else {
            fmt::println("IORING_SETUP_COOP_TASKRUN not supported, requires kernel >= 5.19.0, expect reduced performance.");
        }
        if(_kernelVersion >= v1::Version{6, 0, 0}) {
            p.flags |= IORING_SETUP_SINGLE_ISSUER;
        } else {
            fmt::println("IORING_SETUP_SINGLE_ISSUER not supported, requires kernel >= 6.0.0, expect reduced performance.");
        }
        if(_kernelVersion >= v1::Version{6, 1, 0}) {
            p.flags |= IORING_SETUP_DEFER_TASKRUN;
        } else {
            fmt::println("IORING_SETUP_DEFER_TASKRUN not supported, requires kernel >= 6.1.0, expect reduced performance.");
        }
        p.sq_thread_idle = 50;
        auto ret = io_uring_queue_init_params(entriesCount, _eventQueuePtr, &p);
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
            ret = io_uring_queue_init_params(entriesCount, _eventQueuePtr, &p);
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

    bool IOUringQueue::useEventLoop(v1::NeverNull<io_uring*> event, unsigned entriesCount) {
        if(!checkRingFlags(event)) {
            return false;
        }
        _threadId = std::this_thread::get_id();
        _eventQueuePtr = event;
        _entriesCount = entriesCount;
        _initializedQueue.store(true, std::memory_order_release);
        return true;
    }

    bool IOUringQueue::start(bool captureSigInt) {
        if(!_initializedQueue.load(std::memory_order_acquire)) [[unlikely]] {
            fmt::println("IOUringQueue not initialized. Call createEventLoop or useEventLoop first.");
            return false;
        }

        if(!_dm) [[unlikely]] {
            fmt::println("Please create a manager first!");
            return false;
        }

        if(std::this_thread::get_id() != _threadId) [[unlikely]] {
            fmt::println("Creation of ring and start() have to be on the same thread.");
            std::terminate();
        }
//        fmt::println("start() {} {}", std::hash<std::thread::id>{}(std::this_thread::get_id()), std::hash<std::thread::id>{}(_threadId));

        if(captureSigInt && !Ichor::Detail::registeredSignalHandler.exchange(true)) {
            if(::signal(SIGINT, Ichor::Detail::on_sigint) == SIG_ERR) {
                fmt::println("Couldn't set signal");
                return false;
            }
        }

        addInternalServiceManager(std::make_unique<Detail::InternalServiceLifecycleManager<IIOUringQueue>>(this));

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
            if((unsigned)ret != _entriesCount - space) [[unlikely]] {
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

            auto ret = io_uring_wait_cqes(_eventQueuePtr, &cqe, 1, &ts, nullptr);
//            fmt::println("io_uring_wait_cqe_timeout {} {}", ret, reinterpret_cast<void*>(cqe));
            if(ret < 0 && ret != -ETIME) [[unlikely]] {
                if(ret == -EBADF) {
                    fmt::println("io_uring_wait_cqes() failed, ret {}. Did you create the event loop on the same thread as you started the queue?", -ret);
                } else {
                    fmt::println("io_uring_wait_cqes() failed, ret {}", -ret);
                }
                std::terminate();
            }

            unsigned int handled{};
            unsigned int head{};
            if(ret != -ETIME) {

                io_uring_for_each_cqe(_eventQueuePtr, head, cqe) {
                    TSAN_ANNOTATE_HAPPENS_AFTER(cqe->user_data);
                    auto *evt = reinterpret_cast<Event *>(io_uring_cqe_get_data(cqe));
                    if(evt != nullptr) {
//                        INTERNAL_IO_DEBUG("processing {}", evt->get_name());
                        std::unique_ptr<Event> uniqueEvt{evt};
                        if(evt->get_type() == UringResponseEvent::TYPE) {
                            auto *resEvt = reinterpret_cast<UringResponseEvent*>(evt);
                            resEvt->fun(cqe);
                            if((cqe->flags & IORING_CQE_F_MORE) == IORING_CQE_F_MORE) {
//                                fmt::println("F_MORE {}", evt->id);
                                uniqueEvt.release();
                            }
                        } else {
                            processEvent(uniqueEvt);
                        }

                        if(uniqueEvt) {
                            _pendingEvents.fetch_sub(1, std::memory_order_acq_rel);
                        }
                    }
                    handled++;
                }
                io_uring_cq_advance(_eventQueuePtr, handled);

                {
                    auto space = io_uring_sq_space_left(_eventQueuePtr);
                    auto ready = io_uring_cq_ready(_eventQueuePtr);
                    if(space < _entriesCount && ready == 0u) {

#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
                        beforeSubmitDebug(space);
#endif

                        INTERNAL_IO_DEBUG("submit end of loop {}", _entriesCount - space);
                        ret = io_uring_submit_and_get_events(_eventQueuePtr);
                        if(ret < 0) [[unlikely]] {
                            fmt::println("io_uring_submit_and_get_events() failed, ret {}", -ret);
                            std::terminate();
                        }
                        if((unsigned int)ret != _entriesCount - space) [[unlikely]] {
                            fmt::println("submit wrong amount2 {} {} {}", ret, _entriesCount, space);

#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
                            afterSubmitDebug(ret, space);
#endif

                            std::terminate();
                        }
                    }
                }
            }

            shouldAddQuitEvent();
        }

        // one last loop to de-allocate any potentially unhandled events
//        fmt::println("One last loop");
        bool somethingHandled{true};
        while(somethingHandled) {
            io_uring_cqe *cqe{};
            __kernel_timespec ts{};
            ts.tv_nsec = 500'000;
            unsigned int handled{};
            unsigned int head{};

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
                auto *evt = reinterpret_cast<Event *>(io_uring_cqe_get_data(cqe));
                std::unique_ptr<Event> uniqueEvt{evt};
//                fmt::println("last loop processing {}", evt->get_name());
                if(uniqueEvt) {
                    _pendingEvents.fetch_sub(1, std::memory_order_acq_rel);
                }
                handled++;
            }

            if(handled == 0) {
                somethingHandled = false;
            }
            io_uring_cq_advance(_eventQueuePtr, handled);
        }

        stopDm();

        return true;
    }

    bool IOUringQueue::shouldQuit() {
        if(_quitEventSent.load(std::memory_order_acquire) && std::chrono::steady_clock::now() - _whenQuitEventWasSent >= std::chrono::milliseconds(_quitTimeoutMs)) [[unlikely]] {
            _quit.store(true, std::memory_order_release);
        }

        return _quit.load(std::memory_order_acquire);
    }

    void IOUringQueue::quit() {
        _quit.store(true, std::memory_order_release);
    }

    v1::NeverNull<io_uring*> IOUringQueue::getRing() noexcept {
        if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
            if(_eventQueuePtr == nullptr) [[unlikely]] {
                std::terminate();
            }
        }
        return _eventQueuePtr;
    }

    unsigned int IOUringQueue::getMaxEntriesCount() const noexcept {
        return _entriesCount;
    }

    uint32_t IOUringQueue::sqeSpaceLeft() const noexcept {
        return io_uring_sq_space_left(_eventQueuePtr);
    }

    io_uring_sqe *IOUringQueue::getSqe() noexcept {
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

    io_uring_sqe* IOUringQueue::getSqeWithData(IService *self, std::function<void(io_uring_cqe*)> fun) noexcept {
        INTERNAL_IO_DEBUG("getSqeWithData");
        auto sqe = getSqe();
        io_uring_sqe_set_data(sqe, new UringResponseEvent{getNextEventId(), self->getServiceId(), INTERNAL_EVENT_PRIORITY, std::move(fun)});
        _pendingEvents.fetch_add(1, std::memory_order_acq_rel);
        return sqe;
    }

    io_uring_sqe* IOUringQueue::getSqeWithData(ServiceIdType serviceId, std::function<void(io_uring_cqe*)> fun) noexcept {
        INTERNAL_IO_DEBUG("getSqeWithData");
        auto sqe = getSqe();
        io_uring_sqe_set_data(sqe, new UringResponseEvent{getNextEventId(), serviceId, INTERNAL_EVENT_PRIORITY, std::move(fun)});
        _pendingEvents.fetch_add(1, std::memory_order_acq_rel);
        return sqe;
    }

    void IOUringQueue::submitIfNeeded() {
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
            if((unsigned int)ret != _entriesCount) [[unlikely]] {
                fmt::println("submit wrong amount3 {} {} {}", ret, _entriesCount, space);
#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
                afterSubmitDebug(ret, space);
#endif
                std::terminate();
            }
        }
    }

    void IOUringQueue::forceSubmit() {
        auto space = io_uring_sq_space_left(_eventQueuePtr);
#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
        beforeSubmitDebug(space);
#endif
        auto ret = io_uring_submit(_eventQueuePtr);
        if(ret < 0) [[unlikely]] {
            fmt::println("io_uring_submit() failed, ret {}", -ret);
            std::terminate();
        }
        if((unsigned int)ret != _entriesCount - space) [[unlikely]] {
            fmt::println("submit wrong amount4 {} {} {}", ret, _entriesCount, space);
#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
            afterSubmitDebug(ret, space);
#endif
            std::terminate();
        }
    }

    void IOUringQueue::submitAndWait(uint32_t waitNr) {
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

        if((unsigned int)ret != _entriesCount - space) [[unlikely]] {
            fmt::println("submit wrong amount5 {} {} {}", ret, _entriesCount, space);
#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
            afterSubmitDebug(ret, space);
#endif
            std::terminate();
        }
    }

    v1::Version IOUringQueue::getKernelVersion() const noexcept {
        return _kernelVersion;
    }

    bool IOUringQueue::checkRingFlags(io_uring *ring) {
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
        if(_kernelVersion < v1::Version{6, 7, 0}) {
            fmt::println("SOCKET_URING_OP_GETSOCKOPT/SOCKET_URING_OP_SETSOCKOPT not supported, requires kernel >= 6.7.0, expect reduced socket creation performance.");
        }
        if(_kernelVersion < v1::Version{6, 0, 0}) {
            fmt::println("io_uring_prep_recv_multishot not supported, requires kernel >= 6.0.0, expect reduced socket performance.");
        }
        if(_kernelVersion < v1::Version{5, 19, 0}) {
            fmt::println("io_uring_prep_multishot_accept not supported, requires kernel >= 5.19.0, expect reduced socket performance.");
        }
        return true;
    }

    void IOUringQueue::shouldAddQuitEvent() {
        bool const shouldQuit = Detail::sigintQuit.load(std::memory_order_acquire);

        if(shouldQuit && !_quitEventSent.load(std::memory_order_acquire)) {
            pushEventInternal(INTERNAL_EVENT_PRIORITY, std::make_unique<QuitEvent>(getNextEventId(), 0, INTERNAL_EVENT_PRIORITY));
            _quitEventSent.store(true, std::memory_order_release);
            _whenQuitEventWasSent = std::chrono::steady_clock::now();
        }
    }

    tl::expected<IOUringBuf, v1::IOError> IOUringQueue::createProvidedBuffer(unsigned short entries, unsigned int entryBufferSize) noexcept {
        if(entries == 0) {
            fmt::println("createProvidedBuffer entries == 0.");
            return tl::unexpected(v1::IOError::NOT_SUPPORTED);
        }

        if(entries > 32768) {
            return tl::unexpected(v1::IOError::MESSAGE_SIZE_TOO_BIG);
        }

        if((entries & (entries - 1)) != 0) {
            fmt::println("createProvidedBuffer entries not a power of two.");
            return tl::unexpected(v1::IOError::NOT_SUPPORTED);
        }

        if(_kernelVersion < v1::Version{5, 19, 0}) {
            return tl::unexpected(v1::IOError::KERNEL_TOO_OLD);
        }

        char *entriesBuf = static_cast<char *>(aligned_alloc(static_cast<size_t>(_pageSize), entries * entryBufferSize));
        if(entriesBuf == nullptr) {
            return tl::unexpected(v1::IOError::NO_MEMORY_AVAILABLE);
        }
        if(madvise(entriesBuf, entries * entryBufferSize, MADV_DONTDUMP) != 0) {
            fmt::println("createProvidedBuffer madvise entriesBuf MADV_DONTDUMP failed.");
            return tl::unexpected(v1::IOError::NOT_SUPPORTED);
        }
        if(madvise(entriesBuf, entries * entryBufferSize, MADV_DONTFORK) != 0) {
            fmt::println("createProvidedBuffer madvise entriesBuf MADV_DONTFORK failed.");
            return tl::unexpected(v1::IOError::NOT_SUPPORTED);
        }

        int ret;
        auto id = _uringBufIdCounter++;
        auto *bufRing = io_uring_setup_buf_ring(_eventQueuePtr, entries, id, 0, &ret);
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

        return IOUringBuf(_eventQueuePtr, bufRing, entriesBuf, entries, entryBufferSize, id);
    }
}

#ifdef ICHOR_ENABLE_INTERNAL_URING_DEBUGGING
    void Ichor::IOUringQueue::beforeSubmitDebug(unsigned int space) {
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

    void Ichor::IOUringQueue::afterSubmitDebug(int ret, unsigned int space) {
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
