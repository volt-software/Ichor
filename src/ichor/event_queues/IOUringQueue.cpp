#ifdef ICHOR_USE_LIBURING

#include <ichor/event_queues/IOUringQueue.h>
#include <ichor/ScopeGuard.h>
#include <ichor/DependencyManager.h>
#include <ichor/dependency_management/InternalServiceLifecycleManager.h>
#include <ichor/ichor_liburing.h>

#include <fmt/core.h>
#include <unistd.h>
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


namespace Ichor::Detail {
    extern std::atomic<bool> sigintQuit;
    extern std::atomic<bool> registeredSignalHandler;
    void on_sigint([[maybe_unused]] int sig);
}

namespace Ichor {
    IOUringQueue::IOUringQueue(uint64_t quitTimeoutMs, long long pollTimeoutNs) : _quitTimeoutMs(quitTimeoutMs), _pollTimeoutNs(pollTimeoutNs) {
        if(io_uring_major_version() != 2 || io_uring_minor_version() != 6) {
            fmt::println("io_uring version is {}.{}, but expected 2.6. Ichor is not compiled correctly.", io_uring_major_version(), io_uring_minor_version());
            std::terminate();
        }
        _threadId = std::this_thread::get_id(); // re-set in functions below, because adding events when the queue isn't running yet cannot be done from another thread.
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
            if (::signal(SIGINT, SIG_DFL) == SIG_ERR) {
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

            io_uring_sqe_set_data(sqe, procEvent);
            io_uring_prep_nop(sqe);

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

    io_uring* IOUringQueue::createEventLoop(unsigned entriesCount) {
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
        if (!checkRingFlags(_eventQueuePtr)) {
            return nullptr; // TODO change return type to std::expected with nevernull.
        }
        ret = io_uring_register_ring_fd(_eventQueuePtr);
        if(ret < 0) [[unlikely]] {
            throw std::system_error(-ret, std::generic_category(), "io_uring_register_ring_fd() failed");
        }

        _threadId = std::this_thread::get_id();
        _initializedQueue.store(true, std::memory_order_release);
        return _eventQueuePtr;
    }

    bool IOUringQueue::useEventLoop(io_uring *event, unsigned entriesCount) {
        if (!checkRingFlags(event)) {
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
            fmt::print("Creation of ring and start() have to be on the same thread.");
            std::terminate();
        }
//        fmt::println("start() {} {}", std::hash<std::thread::id>{}(std::this_thread::get_id()), std::hash<std::thread::id>{}(_threadId));

        if(captureSigInt && !Ichor::Detail::registeredSignalHandler.exchange(true)) {
            if (::signal(SIGINT, Ichor::Detail::on_sigint) == SIG_ERR) {
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
            if ((unsigned)ret != _entriesCount - space) [[unlikely]] {
//                fmt::println("io_uring_submit {}", ret);
                fmt::println("submit wrong amount");
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
                io_uring_for_each_cqe(_eventQueuePtr, head, cqe) {
//                    fmt::println("ret != -ETIME {} {} {}", cqe->flags, cqe->res, io_uring_cqe_get_data(cqe));
                    TSAN_ANNOTATE_HAPPENS_AFTER(cqe->user_data);
                    auto *evt = reinterpret_cast<Event *>(io_uring_cqe_get_data(cqe));
                    if (evt != nullptr) {
//                        fmt::println("processing {}", evt->get_name());
                        std::unique_ptr<Event> uniqueEvt{evt};
                        if(uniqueEvt->get_type() == UringResponseEvent::TYPE) {
                            auto *resEvt = reinterpret_cast<UringResponseEvent*>(uniqueEvt.get());
                            resEvt->fun(cqe->res);
                        } else {
                            processEvent(uniqueEvt);
                        }
//                        fmt::println("Done processing {}", evt->get_name());
                    }
                    handled++;
                }

                io_uring_cq_advance(_eventQueuePtr, handled);
                {
                    auto space = io_uring_sq_space_left(_eventQueuePtr);
                    auto ready = io_uring_cq_ready(_eventQueuePtr);
                    if (space < _entriesCount && ready == 0u) {
                        //fmt::println("submit end of loop");
                        ret = io_uring_submit(_eventQueuePtr);
                        if(ret < 0) [[unlikely]] {
                            throw std::system_error(-ret, std::generic_category(), "io_uring_submit() failed.");
                        }
                        if ((unsigned int)ret != _entriesCount - space) [[unlikely]] {
                            fmt::println("submit wrong amount");
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
        if (_quitEventSent.load(std::memory_order_acquire) && std::chrono::steady_clock::now() - _whenQuitEventWasSent >= std::chrono::milliseconds(_quitTimeoutMs)) [[unlikely]] {
            _quit.store(true, std::memory_order_release);
        }

        return _quit.load(std::memory_order_acquire);
    }

    void IOUringQueue::quit() {
        _quit.store(true, std::memory_order_release);
    }

    NeverNull<io_uring*> IOUringQueue::getRing() {
        return _eventQueuePtr;
    }

    unsigned int IOUringQueue::getMaxEntriesCount() {
        return _entriesCount;
    }

    uint64_t IOUringQueue::getNextEventIdFromIchor() {
        return this->IEventQueue::getNextEventId();
    }

    uint32_t IOUringQueue::sqeSpaceLeft() {
        return io_uring_sq_space_left(_eventQueuePtr);
    }

    io_uring_sqe *IOUringQueue::getSqe() {
        auto sqe = io_uring_get_sqe(_eventQueuePtr);
        if(sqe == nullptr) {
            submitIfNeeded();
            sqe = io_uring_get_sqe(_eventQueuePtr);
            if(sqe == nullptr) {
                fmt::println("Couldn't get sqe from ring after submit.");
                std::terminate();
            }
        }
        return sqe;
    }

    void IOUringQueue::submitIfNeeded() {
        auto space = io_uring_sq_space_left(_eventQueuePtr);
        if(space == 0) {
            auto ret = io_uring_submit(_eventQueuePtr);
            //fmt::println("submit");
            if(ret < 0) [[unlikely]] {
                throw std::system_error(-ret, std::generic_category(), "io_uring_submit() failed.");
            }
            if ((unsigned int)ret != _entriesCount) [[unlikely]] {
                fmt::println("submit wrong amount");
                std::terminate();
            }
        }
    }

    void IOUringQueue::submitAndWait(uint32_t waitNr) {
        auto toSubmit = io_uring_sq_ready(_eventQueuePtr);
        auto ret = io_uring_submit_and_wait(_eventQueuePtr, waitNr);

        if(ret < 0) [[unlikely]] {
            fmt::println("pushEventInternal() error waiting for available slots in ring {} {} {}", ret, _entriesCount, toSubmit);
            throw std::system_error(-ret, std::generic_category(), "io_uring_submit_and_wait() failed");
        }

        if ((unsigned int)ret != toSubmit) [[unlikely]] {
            fmt::println("submit wrong amount");
            std::terminate();
        }
    }

    bool IOUringQueue::checkRingFlags(io_uring *ring) {
        if (!(ring->features & IORING_FEAT_FAST_POLL)) {
            fmt::println("IORING_FEAT_FAST_POLL not supported, expect reduced performance.");
        }
        if (!(ring->features & IORING_FEAT_NODROP)) {
            fmt::println("IORING_FEAT_NODROP not supported, cqe overflow will lead to dropped packets!");
        }
        if (!(ring->features & IORING_FEAT_EXT_ARG)) {
            fmt::println("IORING_FEAT_EXT_ARG (a.k.a. IORING_ENTER_EXT_ARG) not supported, expect reduced performance.");
        }
        // it seems all code in Ichor so far keeps state stable until completion, so this shouldn't be necessary.
//        if (!(ring->features & IORING_FEAT_SUBMIT_STABLE)) {
//            fmt::println("IORING_FEAT_SUBMIT_STABLE not supported, kernel too old to use Ichor's io_uring implementation.");
//            return false;
//        }
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
}

#endif
