#ifdef ICHOR_USE_LIBURING

#include <ichor/event_queues/IOUringQueue.h>
#include <ichor/ScopeGuard.h>
#include <ichor/DependencyManager.h>
#include <ichor/dependency_management/InternalServiceLifecycleManager.h>

#include <fmt/core.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/mman.h>
#include <cstdlib>
#include <ichor/event_queues/IIOUringQueue.h>

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

        [[nodiscard]] std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] NameHashType get_type() const noexcept final {
            return TYPE;
        }

        std::function<void(io_uring_cqe*)> fun;
        int32_t res{};
        static constexpr NameHashType TYPE = typeNameHash<UringResponseEvent>();
        static constexpr std::string_view NAME = typeName<UringResponseEvent>();
    };

    IOUringQueue::IOUringQueue(uint64_t quitTimeoutMs, long long pollTimeoutNs, tl::optional<Version> emulateKernelVersion) : _quitTimeoutMs(quitTimeoutMs), _pollTimeoutNs(pollTimeoutNs) {
        if(io_uring_major_version() != 2 || io_uring_minor_version() != 8) {
            fmt::println("io_uring version is {}.{}, but expected 2.7. Ichor is not compiled correctly.", io_uring_major_version(), io_uring_minor_version());
            std::terminate();
        }
        _threadId = std::this_thread::get_id(); // re-set in functions below, because adding events when the queue isn't running yet cannot be done from another thread.

        utsname buffer{};
        if(uname(&buffer) != 0) {
            fmt::println("Couldn't get uname: {}", strerror(errno));
            std::terminate();
        }

        std::string_view release = buffer.release;
        if(auto pos = release.find('-'); pos != std::string_view::npos) {
            release = release.substr(0, pos);
        }

        auto version = parseStringAsVersion(release);

        if(!version) {
            fmt::println("Couldn't parse uname version: {}", buffer.release);
            std::terminate();
        }

        if(*version < Version{5, 4, 0}) {
            fmt::println("Kernel version {} is too old, use at least 5.4.0 or newer.", *version);
            std::terminate();
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


        Event *procEvent = event.release();
//        fmt::println("pushEventInternal {} {}", procEvent->get_name(), reinterpret_cast<void*>(procEvent));
        if(std::this_thread::get_id() != _threadId) [[unlikely]] {
            TSAN_ANNOTATE_HAPPENS_BEFORE(procEvent);
            io_uring tempQueue{};
            io_uring_params p{};
            p.flags = IORING_SETUP_DEFER_TASKRUN | IORING_SETUP_COOP_TASKRUN | IORING_SETUP_SINGLE_ISSUER | IORING_SETUP_ATTACH_WQ;
            p.wq_fd = static_cast<__u32>(_eventQueuePtr->ring_fd);
            auto ret = io_uring_queue_init_params(8, &tempQueue, &p);
            if(ret == -ENOSYS) [[unlikely]] {
                // TODO replace all throws with prints + terminates
                throw std::system_error(-ret, std::generic_category(), "io_uring_queue_init_params() with WQ failed, probably not enabled in the kernel.");
            }
            if(ret < 0) [[unlikely]] {
                p.flags = IORING_SETUP_ATTACH_WQ;
                fmt::println("Couldn't set queue params IORING_SETUP_DEFER_TASKRUN | IORING_SETUP_COOP_TASKRUN | IORING_SETUP_SINGLE_ISSUER, retrying without. Expect reduced performance.");
                ret = io_uring_queue_init_params(8, &tempQueue, &p);
                if(ret < 0) [[unlikely]] {
                    throw std::system_error(-ret, std::generic_category(), "io_uring_queue_init_params() for tempQueue failed");
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
                fmt::println("io_uring_submit_and_wait {}", ret);
                fmt::println("submit wrong amount, would have resulted in dropping event");
                std::terminate();
            }

//            fmt::println("pushEventInternal() {} {}", std::hash<std::thread::id>{}(std::this_thread::get_id()), std::hash<std::thread::id>{}(_threadId));

//            std::terminate();
        } else {
            auto *sqe = io_uring_get_sqe(_eventQueuePtr);

            if(sqe == nullptr) {
                auto ret = io_uring_submit_and_wait(_eventQueuePtr, 2);

                if(ret < 0) {
                    auto space = io_uring_sq_space_left(_eventQueuePtr);
                    fmt::println("pushEventInternal() error waiting for available slots in ring {} {} {}", ret, _entriesCount, space);
                    throw std::system_error(-ret, std::generic_category(), "io_uring_submit_and_wait() failed.");
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

        // TODO
        return !_dm || !_dm->isRunning();
    }

    uint64_t IOUringQueue::size() const {
        if(!_initializedQueue.load(std::memory_order_acquire)) [[unlikely]] {
            fmt::println("IOUringQueue not initialized. Call createEventLoop or useEventLoop first.");
            std::terminate();
        }

        // TODO
        return (_dm && _dm->isRunning()) ? 1 : 0;
    }

    bool IOUringQueue::is_running() const noexcept {
        return !_quit.load(std::memory_order_acquire);
    }

    tl::optional<NeverNull<io_uring*>> IOUringQueue::createEventLoop(unsigned entriesCount) {
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
        p.flags = IORING_SETUP_DEFER_TASKRUN | IORING_SETUP_COOP_TASKRUN | IORING_SETUP_SINGLE_ISSUER;
        p.sq_thread_idle = 50;
        auto ret = io_uring_queue_init_params(entriesCount, _eventQueuePtr, &p);
        if(ret == -ENOSYS) [[unlikely]] {
            throw std::system_error(-ret, std::generic_category(), "io_uring_queue_init_params() failed, probably not enabled in the kernel.");
        }
        if(ret < 0) [[unlikely]] {
            p.flags = 0;
            fmt::println("Couldn't set queue params IORING_SETUP_DEFER_TASKRUN | IORING_SETUP_COOP_TASKRUN | IORING_SETUP_SINGLE_ISSUER, retrying without. Expect reduced performance.");
            ret = io_uring_queue_init_params(entriesCount, _eventQueuePtr, &p);
            if(ret < 0) [[unlikely]] {
                throw std::system_error(-ret, std::generic_category(), "io_uring_queue_init_params() failed");
            }
        }
        if(!checkRingFlags(_eventQueuePtr)) {
            return {};
        }
        ret = io_uring_register_ring_fd(_eventQueuePtr);
        if(ret < 0) [[unlikely]] {
            throw std::system_error(-ret, std::generic_category(), "io_uring_register_ring_fd() failed");
        }
        ret = io_uring_ring_dontfork(_eventQueuePtr);
        if(ret < 0) [[unlikely]] {
            throw std::system_error(-ret, std::generic_category(), "io_uring_ring_dontfork() failed");
        }
		ret = io_uring_ring_dontdump(_eventQueuePtr);
		if(ret < 0) [[unlikely]] {
			throw std::system_error(errno, std::generic_category(), "io_uring_ring_dontdump() failed");
		}
		ret = io_uring_ring_mlock(_eventQueuePtr);
		if(ret < 0) [[unlikely]] {
			fmt::println("warning: io_uring_ring_mlock() failed: {}", errno);
		}

//        _priorityQueue.reserve(entriesCount);
        _threadId = std::this_thread::get_id();
        _initializedQueue.store(true, std::memory_order_release);
        return _eventQueuePtr;
    }

    bool IOUringQueue::useEventLoop(NeverNull<io_uring*> event, unsigned entriesCount) {
        if(!checkRingFlags(event)) {
            return false;
        }
        _threadId = std::this_thread::get_id();
        _eventQueuePtr = event;
        _entriesCount = entriesCount;
//        _priorityQueue.reserve(entriesCount);
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
            fmt::print("Creation of ring and start() have to be on the same thread.");
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
            if(ret < 0) [[unlikely]] {
                throw std::system_error(-ret, std::generic_category(), "io_uring_submit() failed.");
            }
            if((unsigned)ret != _entriesCount - space) [[unlikely]] {
//                fmt::println("io_uring_submit {}", ret);
                fmt::println("submit wrong amount1");
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
                    throw std::system_error(-ret, std::generic_category(), "io_uring_wait_cqes() failed. Did you create the event loop on the same thread as you started the queue?");
                } else {
                    throw std::system_error(-ret, std::generic_category(), "io_uring_wait_cqes() failed");
                }
            }

            unsigned int handled{};
            unsigned int head{};
            if(ret != -ETIME) {
                // attempt at forcing some sort of priority for events, though with cqe's being a non-continuous range, impossible to guarantee.
                // also takes a significant performance hit.
#if 0
                io_uring_for_each_cqe(_eventQueuePtr, head, cqe) {
                    TSAN_ANNOTATE_HAPPENS_AFTER(cqe->user_data);
                    auto *evt = reinterpret_cast<Event *>(io_uring_cqe_get_data(cqe));
                    if(evt != nullptr) {
                        if(evt->get_type() == UringResponseEvent::TYPE) {
                            auto *resEvt = reinterpret_cast<UringResponseEvent*>(evt);
                            resEvt->res = cqe->res;
                        }
                        _priorityQueue.emplace(evt);
                    }
                    handled++;
                }
                io_uring_cq_advance(_eventQueuePtr, handled);

                while(!_priorityQueue.empty()) {
                    auto evt = _priorityQueue.pop();
                    if(!evt) {
                        std::terminate();
                    }
                    if(evt->get_type() == UringResponseEvent::TYPE) {
                        auto *resEvt = reinterpret_cast<UringResponseEvent*>(evt.get());
                        resEvt->fun(resEvt->res);
                    } else {
                        processEvent(evt);
                    }
                }
#else

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
                    }
                    handled++;
                }
                io_uring_cq_advance(_eventQueuePtr, handled);
#endif

                {
                    auto space = io_uring_sq_space_left(_eventQueuePtr);
                    auto ready = io_uring_cq_ready(_eventQueuePtr);
                    if(space < _entriesCount && ready == 0u) {
                        INTERNAL_IO_DEBUG("submit end of loop {}", _entriesCount - space);
                        ret = io_uring_submit_and_get_events(_eventQueuePtr);
                        if(ret < 0) [[unlikely]] {
                            throw std::system_error(-ret, std::generic_category(), "io_uring_submit_and_get_events() failed.");
                        }
                        if((unsigned int)ret != _entriesCount - space) [[unlikely]] {
                            fmt::println("submit wrong amount2 {} {} {}", ret, _entriesCount, space);
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
                throw std::system_error(-ret, std::generic_category(), "io_uring_wait_cqes() failed.");
            }
            ret = io_uring_get_events(_eventQueuePtr);
            if(ret < 0) {
                throw std::system_error(-ret, std::generic_category(), "io_uring_get_events() failed.");
            }
            io_uring_for_each_cqe(_eventQueuePtr, head, cqe) {
                TSAN_ANNOTATE_HAPPENS_AFTER(cqe->user_data);
                auto *evt = reinterpret_cast<Event *>(io_uring_cqe_get_data(cqe));
                std::unique_ptr<Event> uniqueEvt{evt};
//                fmt::println("last loop processing {}", evt->get_name());
                handled++;
            }

            if(handled == 0) {
                somethingHandled = false;
            }
            io_uring_cq_advance(_eventQueuePtr, handled);
        }

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

    NeverNull<io_uring*> IOUringQueue::getRing() noexcept {
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
        return sqe;
    }

    void IOUringQueue::submitIfNeeded() {
        auto space = io_uring_sq_space_left(_eventQueuePtr);
        if(space == 0) {
//            fmt::println("submit {}", _entriesCount);
            auto ret = io_uring_submit(_eventQueuePtr);
            if(ret < 0) [[unlikely]] {
                throw std::system_error(-ret, std::generic_category(), "io_uring_submit() failed.");
            }
            if((unsigned int)ret != _entriesCount) [[unlikely]] {
                fmt::println("submit wrong amount3");
                std::terminate();
            }
        }
    }

    void IOUringQueue::forceSubmit() {
        auto ret = io_uring_submit(_eventQueuePtr);
        if(ret < 0) [[unlikely]] {
            throw std::system_error(-ret, std::generic_category(), "io_uring_submit() failed.");
        }
        if((unsigned int)ret != _entriesCount) [[unlikely]] {
            fmt::println("submit wrong amount4");
            std::terminate();
        }
    }

    void IOUringQueue::submitAndWait(uint32_t waitNr) {
        auto toSubmit = io_uring_sq_ready(_eventQueuePtr);
//        fmt::println("submit and wait {}", toSubmit);
        auto ret = io_uring_submit_and_wait(_eventQueuePtr, waitNr);

        if(ret < 0) [[unlikely]] {
            fmt::println("pushEventInternal() error waiting for available slots in ring {} {} {}", ret, _entriesCount, toSubmit);
            throw std::system_error(-ret, std::generic_category(), "io_uring_submit_and_wait() failed");
        }

        if((unsigned int)ret != toSubmit) [[unlikely]] {
            fmt::println("submit wrong amount5");
            std::terminate();
        }
    }

    Version IOUringQueue::getKernelVersion() const noexcept {
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
        if(_kernelVersion < Version{6, 7, 0}) {
            fmt::println("SOCKET_URING_OP_GETSOCKOPT/SOCKET_URING_OP_SETSOCKOPT not supported, requires kernel >= 6.7.0, expect reduced socket creation performance.");
        }
        if(_kernelVersion < Version{6, 0, 0}) {
            fmt::println("io_uring_prep_recv_multishot not supported, requires kernel >= 6.0.0, expect reduced socket performance.");
        }
        if(_kernelVersion < Version{5, 19, 0}) {
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

    tl::expected<IOUringBuf, IOError> IOUringQueue::createProvidedBuffer(unsigned short entries, unsigned int entryBufferSize) noexcept {
        if(entries == 0) {
			fmt::println("createProvidedBuffer entries == 0.");
            return tl::unexpected(IOError::NOT_SUPPORTED);
        }

        if(entries > 32768) {
            return tl::unexpected(IOError::MESSAGE_SIZE_TOO_BIG);
        }

        if((entries & (entries - 1)) != 0) {
			fmt::println("createProvidedBuffer entries not a power of two.");
            return tl::unexpected(IOError::NOT_SUPPORTED);
        }

        if(_kernelVersion < Version{5, 19, 0}) {
            return tl::unexpected(IOError::KERNEL_TOO_OLD);
        }

        char *entriesBuf = static_cast<char *>(aligned_alloc(static_cast<size_t>(_pageSize), entries * entryBufferSize));
        if(entriesBuf == nullptr) {
            return tl::unexpected(IOError::NO_MEMORY_AVAILABLE);
        }
		if(madvise(entriesBuf, entries * entryBufferSize, MADV_DONTDUMP) != 0) {
			fmt::println("createProvidedBuffer madvise entriesBuf MADV_DONTDUMP failed.");
			return tl::unexpected(IOError::NOT_SUPPORTED);
		}
		if(madvise(entriesBuf, entries * entryBufferSize, MADV_DONTFORK) != 0) {
			fmt::println("createProvidedBuffer madvise entriesBuf MADV_DONTFORK failed.");
			return tl::unexpected(IOError::NOT_SUPPORTED);
		}

        int ret;
        auto id = _uringBufIdCounter++;
        auto *bufRing = io_uring_setup_buf_ring(_eventQueuePtr, entries, id, 0, &ret);
        if(bufRing == nullptr) {
            return tl::unexpected(mapErrnoToError(-ret));
        }
		if(madvise(bufRing, entries * sizeof(struct io_uring_buf), MADV_DONTDUMP) != 0) {
			fmt::println("createProvidedBuffer madvise bufRing MADV_DONTDUMP failed.");
			return tl::unexpected(IOError::NOT_SUPPORTED);
		}
		if(madvise(bufRing, entries * sizeof(struct io_uring_buf), MADV_DONTFORK) != 0) {
			fmt::println("createProvidedBuffer madvise bufRing MADV_DONTFORK failed.");
			return tl::unexpected(IOError::NOT_SUPPORTED);
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

#endif
