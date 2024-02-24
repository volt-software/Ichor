#ifdef ICHOR_USE_LIBURING

#include <ichor/event_queues/IOUringQueue.h>
#include <ichor/ScopeGuard.h>
#include <ichor/DependencyManager.h>

// liburing uses different conventions than Ichor, ignore them to prevent being spammed by warnings
#if defined( __GNUC__ )
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wsign-conversion"
#    pragma GCC diagnostic ignored "-Wshadow"
#    pragma GCC diagnostic ignored "-Wconversion"
#    pragma GCC diagnostic ignored "-Wpedantic"
#    pragma GCC diagnostic ignored "-Wgnu-pointer-arith"
#    pragma GCC diagnostic ignored "-Wgnu-anonymous-struct"
#    pragma GCC diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
#    pragma GCC diagnostic ignored "-Wimplicit-int-conversion"
#    pragma GCC diagnostic ignored "-Wnested-anon-types"
#    pragma GCC diagnostic ignored "-Wzero-length-array"
#    pragma GCC diagnostic ignored "-Wcast-align"
#    pragma GCC diagnostic ignored "-Wc99-extensions"
#endif
#include "liburing.h"
#if defined( __GNUC__ )
#    pragma GCC diagnostic pop
#endif

//#include <spdlog/spdlog.h>

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
        _threadId = std::this_thread::get_id();

        if(!io_uring_check_version(2, 5)) {
//            spdlog::info("io_uring version is not 2,5. Ichor is not compiled correctly.");
            std::terminate();
        }
    }

    IOUringQueue::~IOUringQueue() {
        if(_initializedQueue.load(std::memory_order_acquire)) {
            stopDm();
            TSAN_ANNOTATE_HAPPENS_AFTER(_eventQueuePtr);
            if(_eventQueue) {
                io_uring_queue_exit(_eventQueue.get());
            }
        }

        if(Detail::registeredSignalHandler) {
            if (::signal(SIGINT, SIG_DFL) == SIG_ERR) {
//                spdlog::info("Couldn't unset signal handler");
            }
        }
    }

    void IOUringQueue::pushEventInternal(uint64_t priority, std::unique_ptr<Event> &&event) {
        if(!_initializedQueue.load(std::memory_order_acquire)) [[unlikely]] {
            throw std::runtime_error("sdevent not initialized. Call createEventLoop or useEventLoop first.");
        }

        if(!event) [[unlikely]] {
            throw std::runtime_error("Pushing nullptr");
        }

        if(shouldQuit()) {
//            spdlog::info("pushEventInternal, should quit! not inserting {}", event->get_name());
            return;
        }

        // TODO hardening
//#ifdef ICHOR_USE_HARDENING
//            if(originatingServiceId != 0 && _services.find(originatingServiceId) == _services.end()) [[unlikely]] {
//                std::terminate();
//            }
//#endif


        Event *procEvent = event.release();
        //spdlog::info("pushEventInternal {} {}", procEvent->get_name(), reinterpret_cast<void*>(procEvent));
        if(std::this_thread::get_id() != _threadId) [[unlikely]] {
            TSAN_ANNOTATE_HAPPENS_BEFORE(procEvent);
            io_uring tempQueue{};
            io_uring_params p{};
            p.flags = IORING_SETUP_DEFER_TASKRUN | IORING_SETUP_COOP_TASKRUN | IORING_SETUP_SINGLE_ISSUER | IORING_SETUP_ATTACH_WQ;
            p.wq_fd = static_cast<__u32>(_eventQueuePtr->ring_fd);
            auto ret = io_uring_queue_init_params(8, &tempQueue, &p);
            if(ret < 0) [[unlikely]] {
                throw std::system_error(-ret, std::generic_category(), "io_uring_queue_init_params() failed");
            }
            ScopeGuard sg{[&tempQueue]() {
                io_uring_queue_exit(&tempQueue);
            }};

            auto sqe = io_uring_get_sqe(&tempQueue);
            io_uring_prep_msg_ring(sqe, _eventQueuePtr->ring_fd, 0, reinterpret_cast<uintptr_t>(reinterpret_cast<void*>(procEvent)), 0);
            TSAN_ANNOTATE_HAPPENS_BEFORE(_eventQueuePtr);
            ret = io_uring_submit_and_wait(&tempQueue, 1);
            if(ret != 1) [[unlikely]] {
//                spdlog::info("io_uring_submit {}", ret);
                throw std::runtime_error("submit wrong amount, would have resulted in dropping event");
            }
        } else {
            auto *sqe = io_uring_get_sqe(_eventQueuePtr);
            io_uring_sqe_set_data(sqe, procEvent);
            io_uring_prep_nop(sqe);

            auto space = io_uring_sq_space_left(_eventQueuePtr);
            if(space == 0) {
                auto ret = io_uring_submit(_eventQueuePtr);
                if (ret != _entriesCount) [[unlikely]] {
//                spdlog::info("io_uring_submit {}", ret);
                    throw std::runtime_error("submit wrong amount");
                }
            }
        }
    }

    bool IOUringQueue::empty() const {
        if(!_initializedQueue.load(std::memory_order_acquire)) [[unlikely]] {
            throw std::runtime_error("sdevent not initialized. Call createEventLoop or useEventLoop first.");
        }

        // TODO
        return !_dm->isRunning();
    }

    uint64_t IOUringQueue::size() const {
        if(!_initializedQueue.load(std::memory_order_acquire)) [[unlikely]] {
            throw std::runtime_error("sdevent not initialized. Call createEventLoop or useEventLoop first.");
        }

        // TODO
        return _dm->isRunning() ? 1 : 0;
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
        if(ret < 0) [[unlikely]] {
            throw std::system_error(-ret, std::generic_category(), "io_uring_queue_init_params() failed");
        }
        if (!(p.features & IORING_FEAT_FAST_POLL)) {
            fmt::print("IORING_FEAT_FAST_POLL not supported, expect reduced performance\n");
        }
        ret = io_uring_register_ring_fd(_eventQueuePtr);
        if(ret < 0) [[unlikely]] {
            throw std::system_error(-ret, std::generic_category(), "io_uring_register_ring_fd() failed");
        }

        _initializedQueue.store(true, std::memory_order_release);
        return _eventQueuePtr;
    }

    void IOUringQueue::useEventLoop(io_uring *event, unsigned entriesCount) {
        _eventQueuePtr = event;
        _entriesCount = entriesCount;
        _initializedQueue.store(true, std::memory_order_release);
    }

    void IOUringQueue::start(bool captureSigInt) {
        if(!_initializedQueue.load(std::memory_order_acquire)) [[unlikely]] {
            throw std::runtime_error("sdevent not initialized. Call createEventLoop or useEventLoop first.");
        }

        if(!_dm) [[unlikely]] {
            throw std::runtime_error("Please create a manager first!");
        }

        // this capture currently has no way to wake all queues. Multimap f.e. polls sigintQuit, but with sdevent the
        if(captureSigInt && !Ichor::Detail::registeredSignalHandler.exchange(true)) {
            if (::signal(SIGINT, Ichor::Detail::on_sigint) == SIG_ERR) {
                throw std::runtime_error("Couldn't set signal");
            }
        }

        startDm();

        {
            auto space = io_uring_sq_space_left(_eventQueuePtr);
            auto ret = io_uring_submit(_eventQueuePtr);
            if (ret != _entriesCount - space) [[unlikely]] {
//                spdlog::info("io_uring_submit {}", ret);
                throw std::runtime_error("submit wrong amount");
            }
        }

        while(!shouldQuit()) [[likely]] {
            io_uring_cqe *cqe{};
            __kernel_timespec ts{};
            ts.tv_nsec = _pollTimeoutNs;

            auto ret = io_uring_wait_cqe_timeout(_eventQueuePtr, &cqe, &ts);
//            spdlog::info("io_uring_wait_cqe_timeout {} {}", ret, reinterpret_cast<void*>(cqe));
            if(ret < 0 && ret != -ETIME) [[unlikely]] {
                throw std::system_error(-ret, std::generic_category(), "io_uring_wait_cqe_timeout() failed");
            }

            if(ret != -ETIME) {
//                spdlog::info("ret != -ETIME {} {} {}", cqe->flags, cqe->res, io_uring_cqe_get_data(cqe));
                TSAN_ANNOTATE_HAPPENS_AFTER(cqe->user_data);
                auto *evt = reinterpret_cast<Event*>(io_uring_cqe_get_data(cqe));
                if(evt != nullptr) {
                    //spdlog::info("processing {}", evt->get_name());
                    std::unique_ptr<Event> uniqueEvt{evt};
                    processEvent(uniqueEvt);

                    {
                        auto space = io_uring_sq_space_left(_eventQueuePtr);
                        if(space < _entriesCount) {
                            ret = io_uring_submit(_eventQueuePtr);
                            if (ret != _entriesCount - space) [[unlikely]] {
//                          spdlog::info("io_uring_submit {}", ret);
                                throw std::runtime_error("submit wrong amount");
                            }
                        }
                    }
                }
            }

            if(_eventQueuePtr != nullptr) {
                io_uring_cqe_seen(_eventQueuePtr, cqe);
            }

            shouldAddQuitEvent();
        }
    }

    bool IOUringQueue::shouldQuit() {
        if (_quitEventSent.load(std::memory_order_acquire) && std::chrono::steady_clock::now() - _whenQuitEventWasSent >= std::chrono::milliseconds(_quitTimeoutMs)) [[unlikely]] {
            _quit.store(true, std::memory_order_release);
        }

        return _quit.load(std::memory_order_acquire);
    }
    void IOUringQueue::shouldAddQuitEvent() {
        bool const shouldQuit = Detail::sigintQuit.load(std::memory_order_acquire);

        if(shouldQuit && !_quitEventSent.load(std::memory_order_acquire)) {
            // assume _eventQueueMutex is locked
            pushEventInternal(INTERNAL_EVENT_PRIORITY, std::make_unique<QuitEvent>(getNextEventId(), 0, INTERNAL_EVENT_PRIORITY));
            _quitEventSent.store(true, std::memory_order_release);
            _whenQuitEventWasSent = std::chrono::steady_clock::now();
        }
    }

    void IOUringQueue::quit() {
//        spdlog::info("quit()");
        if(!_quitEventSent.load(std::memory_order_acquire)) {
            // assume _eventQueueMutex is locked
            pushEventInternal(INTERNAL_EVENT_PRIORITY, std::make_unique<QuitEvent>(getNextEventId(), 0, INTERNAL_EVENT_PRIORITY));
            _quitEventSent.store(true, std::memory_order_release);
            _whenQuitEventWasSent = std::chrono::steady_clock::now();
        }
    }
}

#endif
