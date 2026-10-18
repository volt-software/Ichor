#include "Common.h"
#include <ichor/ScopeGuard.h>
#include "TestEvents.h"
#include "TestServices/UselessService.h"
#include <ichor/event_queues/PriorityQueue.h>
#ifdef ICHOR_USE_SDEVENT
#include <ichor/event_queues/SdeventQueue.h>
#endif
#ifdef ICHOR_USE_LIBURING
#include <ichor/event_queues/IOUringQueue.h>
#include <ichor/stl/LinuxUtils.h>
#include "TestServices/IOUringSleepService.h"
#endif
#ifdef __linux__
#include <unistd.h>
#endif
#ifdef ICHOR_USE_BOOST_BEAST
#include <ichor/event_queues/BoostAsioQueue.h>
#endif

#ifdef ICHOR_USE_LIBURING
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/ichor_liburing.h>
#include <vector>
#include <algorithm>
#include <csignal>
#include <sys/wait.h>

namespace {
    /// Runs fn in a forked child, true if the child aborted (e.g. std::terminate from a hardening check).
    template <typename F>
    bool abortsInChild(F &&fn) {
        pid_t const pid = fork();
        if(pid == 0) {
            fn();
            _exit(0);
        }
        int status{};
        waitpid(pid, &status, 0);
        return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
    }

    struct CountedEvent final : public Event {
        explicit CountedEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority) noexcept :
                Event(_id, _originatingService, _priority) {
            created++;
        }
        ~CountedEvent() final {
            destroyed++;
        }

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
            return TYPE;
        }

        static constexpr NameHashType TYPE = typeNameHash<CountedEvent>();
        static constexpr std::string_view NAME = typeName<CountedEvent>();
        static inline std::atomic<uint64_t> created{};
        static inline std::atomic<uint64_t> destroyed{};
    };

    /// Pushes an event while stopping, which happens while handling the queue's last batch of events on quit.
    struct PushesEventOnStopService final : public AdvancedService<PushesEventOnStopService> {
        PushesEventOnStopService() = default;

        Task<void> stop() final {
            GetThreadLocalEventQueue().pushEvent<CountedEvent>(getServiceId());
            co_return;
        }
    };
}
#endif

TEST_CASE("QueueTests") {

    SECTION("PriorityQueue") {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->getManager();

#if ICHOR_EXCEPTIONS_ENABLED
        REQUIRE_THROWS(queue->pushEventInternal(0, nullptr));
#endif

        REQUIRE(queue->empty());
        REQUIRE(queue->size() == 0);
        REQUIRE(!queue->shouldQuit());

#if ICHOR_EXCEPTIONS_ENABLED
        REQUIRE_NOTHROW(queue->pushEventInternal(10, std::make_unique<TestEvent>(0, ServiceIdType{0}, 10)));
#else
        queue->pushEventInternal(10, std::make_unique<TestEvent>(0, ServiceIdType{0}, 10));
#endif

        REQUIRE(!queue->empty());
        REQUIRE(queue->size() == 1);

        queue->quit();

        REQUIRE(!queue->empty());
        REQUIRE(queue->size() == 1);
        REQUIRE(queue->shouldQuit());
    }

    SECTION("OrderedPriorityQueue") {
        auto queue = std::make_unique<OrderedPriorityQueue>();
        auto &dm = queue->getManager();

#if ICHOR_EXCEPTIONS_ENABLED
        REQUIRE_THROWS(queue->pushEventInternal(0, nullptr));
#endif

        REQUIRE(queue->empty());
        REQUIRE(queue->size() == 0);
        REQUIRE(!queue->shouldQuit());

#if ICHOR_EXCEPTIONS_ENABLED
        REQUIRE_NOTHROW(queue->pushEventInternal(10, std::make_unique<TestEvent>(0, ServiceIdType{0}, 10)));
#else
        queue->pushEventInternal(10, std::make_unique<TestEvent>(0, ServiceIdType{0}, 10));
#endif

        REQUIRE(!queue->empty());
        REQUIRE(queue->size() == 1);

        queue->quit();

        REQUIRE(!queue->empty());
        REQUIRE(queue->size() == 1);
        REQUIRE(queue->shouldQuit());
    }

//     SECTION("Delete of uninitialized queues") {
//         auto f = []() {
//             std::array<PriorityQueue, 8> queues{};
//         };
// #if ICHOR_EXCEPTIONS_ENABLED
//         REQUIRE_NOTHROW(f());
// #else
//         f();
// #endif
//     }

#ifdef ICHOR_USE_SDEVENT
    SECTION("SdeventQueue") {
        auto queue = std::make_unique<SdeventQueue>();
        auto &dm = queue->getManager();

        Ichor::v1::Detail::_local_dm = &dm;

#if ICHOR_EXCEPTIONS_ENABLED
        REQUIRE_THROWS(queue->pushEventInternal(0, nullptr));
        REQUIRE_THROWS(queue->empty());
        REQUIRE_THROWS(queue->size());
#endif

        auto loop = queue->createEventLoop();

        REQUIRE(queue->empty());
        REQUIRE(queue->size() == 0);
        REQUIRE(!queue->shouldQuit());

        queue->quit();

        REQUIRE(queue->shouldQuit());
    }

    SECTION("SdeventQueue Live") {
        std::atomic<DependencyManager*> _dm{};
        std::thread t([&] {
            auto queue = std::make_unique<SdeventQueue>();
            auto &dm = queue->getManager();
            _dm.store(&dm, std::memory_order_release);

            auto *loop = queue->createEventLoop();
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService>();
            queue->start(DoNotCaptureSigInt);

            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
        });

        while(_dm.load(std::memory_order_acquire) == nullptr) {
            std::this_thread::sleep_for(1ms);
        }
        auto *dm = _dm.load(std::memory_order_acquire);

        dm->runForOrQueueEmpty();

        REQUIRE(dm->isRunning());

#if ICHOR_EXCEPTIONS_ENABLED
        try {
#endif
            dm->getEventQueue().pushEvent<QuitEvent>(ServiceIdType{0});
#if ICHOR_EXCEPTIONS_ENABLED
        } catch(const std::exception &e) {
            fmt::print("exception: {}\n", e.what());
            throw;
        }
#endif

        t.join();
    }
#endif

#ifdef ICHOR_USE_BOOST_BEAST
    SECTION("BoostAsioQueue") {
        auto queue = std::make_unique<BoostAsioQueue>();
        auto &dm = queue->getManager();

        Ichor::v1::Detail::_local_dm = &dm;

        // REQUIRE_THROWS(queue->pushEventInternal(0, nullptr));
        // REQUIRE_THROWS(queue->empty());
        // REQUIRE_THROWS(queue->size());

        REQUIRE(queue->empty());
        REQUIRE(queue->size() == 0);
        REQUIRE(!queue->shouldQuit());

        queue->quit();

        REQUIRE(queue->shouldQuit());
    }

    SECTION("BoostAsioQueue Live") {
        std::atomic<DependencyManager*> _dm{};
        std::thread t([&] {
            auto queue = std::make_unique<BoostAsioQueue>();
            auto &dm = queue->getManager();
            _dm.store(&dm, std::memory_order_release);

            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService>();
            queue->start(DoNotCaptureSigInt);
        });

        while(_dm.load(std::memory_order_acquire) == nullptr) {
            std::this_thread::sleep_for(1ms);
        }
        auto *dm = _dm.load(std::memory_order_acquire);

        dm->runForOrQueueEmpty();

        REQUIRE(dm->isRunning());

#if ICHOR_EXCEPTIONS_ENABLED
        try {
#endif
            dm->getEventQueue().pushEvent<QuitEvent>(ServiceIdType{0});
#if ICHOR_EXCEPTIONS_ENABLED
        } catch(const std::exception &e) {
            fmt::print("exception: {}\n", e.what());
            throw;
        }
#endif

        t.join();
    }
#endif

// running this inside docker for aarch64 may cause problems, but we still want to test compilation on those setups
#if defined(ICHOR_USE_LIBURING) && !(defined(ICHOR_SKIP_EXTERNAL_TESTS) && defined(ICHOR_AARCH64))
    SECTION("IOUringQueue") {
        auto queue = std::make_unique<IOUringQueue>();
        auto &dm = queue->getManager();

        Ichor::v1::Detail::_local_dm = &dm;

//        REQUIRE_THROWS(queue->pushEventInternal(0, nullptr));
//        REQUIRE_THROWS(queue->empty());
//        REQUIRE_THROWS(queue->size());

        REQUIRE(queue->createEventLoop());
        REQUIRE(queue->empty());
        REQUIRE(queue->size() == 0);
        REQUIRE(!queue->shouldQuit());
    }

    SECTION("IOUringQueue provided buffer of 4 GiB") {
        auto version = Ichor::v1::kernelVersion();
        REQUIRE(version);
        if(version < Version{.major=5, .minor=19, .patch=0}) {
            SKIP("provided buffer rings require kernel 5.19");
        }

        auto queue = std::make_unique<IOUringQueue>(10, 10'000);
        REQUIRE(queue->createEventLoop());

        {
            // 32768 entries of 128 KiB: the size of the whole buffer does not fit in 32 bits
            constexpr unsigned short entries = 32768;
            constexpr unsigned int entryBufferSize = 128 * 1024;
            auto buf = queue->createProvidedBuffer(entries, entryBufferSize);
            if constexpr (sizeof(size_t) < 8) {
                REQUIRE(!buf);
                REQUIRE(buf.error() == IOError::MESSAGE_SIZE_TOO_BIG);
            } else {
                REQUIRE(buf);
                auto lastEntry = buf->readMemory(entries - 1);
                REQUIRE(lastEntry.size() == entryBufferSize);
                // Only readable if the last entry is part of the allocation
                volatile char lastByte = lastEntry.back();
                (void)lastByte;
            }
        }
    }

    SECTION("IOUringQueue provided buffer entries outside of the buffer terminate") {
        auto version = Ichor::v1::kernelVersion();
        REQUIRE(version);
        if(version < Version{.major=5, .minor=19, .patch=0}) {
            SKIP("provided buffer rings require kernel 5.19");
        }

        auto queue = std::make_unique<IOUringQueue>(10, 10'000);
        REQUIRE(queue->createEventLoop());

        {
            constexpr unsigned short entries = 8;
            auto buf = queue->createProvidedBuffer(entries, 4096);
            REQUIRE(buf);
            REQUIRE(!abortsInChild([&]() { (void)buf->readMemory(entries - 1); }));
            REQUIRE(abortsInChild([&]() { (void)buf->readMemory(entries); }));
            REQUIRE(abortsInChild([&]() { buf->markEntryAvailableAgain(entries); }));
        }
    }

    SECTION("IOUringQueue provided buffer frees its memory when registering it fails") {
        auto version = Ichor::v1::kernelVersion();
        REQUIRE(version);
        if(version < Version{.major=5, .minor=19, .patch=0}) {
            SKIP("provided buffer rings require kernel 5.19");
        }

        auto queue = std::make_unique<IOUringQueue>(10, 10'000);
        auto ring = queue->createEventLoop();
        REQUIRE(ring);

        // Take the buffer group id createProvidedBuffer will use next, so that registering its ring fails. A leak shows up in the sanitizer builds.
        constexpr unsigned short entries = 8;
        int ret{};
        auto *takenBufRing = io_uring_setup_buf_ring(ring->get(), entries, 1, 0, &ret);
        REQUIRE(takenBufRing != nullptr);

        auto buf = queue->createProvidedBuffer(entries, 4096);
        REQUIRE(!buf);

        io_uring_free_buf_ring(ring->get(), takenBufRing, entries, 1);
    }

    SECTION("IOUringQueue Live") {
        auto version = Ichor::v1::kernelVersion();

        REQUIRE(version);
        if(version < Version{.major=5, .minor=18, .patch=0}) {
            return;
        }

        auto queue = std::make_unique<IOUringQueue>(10, 10'000);
        std::atomic<DependencyManager*> _dm{};
        Ichor::v1::Detail::_local_dm = nullptr;
        std::thread t([&] {
            REQUIRE(queue->createEventLoop());
            auto &dm = queue->getManager();
            Ichor::v1::Detail::_local_dm = &dm;
            _dm.store(&dm, std::memory_order_release);

            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService>();
            queue->start(DoNotCaptureSigInt);
        });

        while(_dm.load(std::memory_order_acquire) == nullptr) {
            std::this_thread::sleep_for(1ms);
        }
        auto *dm = _dm.load(std::memory_order_acquire);

        dm->runForOrQueueEmpty();

        REQUIRE(dm->isRunning());

#if ICHOR_EXCEPTIONS_ENABLED
        try {
#endif
            dm->getEventQueue().pushEvent<QuitEvent>(ServiceIdType{0});
#if ICHOR_EXCEPTIONS_ENABLED
        } catch(const std::exception &e) {
            fmt::print("exception: {}\n", e.what());
            throw;
        }
#endif

        auto start = std::chrono::steady_clock::now();
        while(queue->is_running()) {
            std::this_thread::sleep_for(10ms);
            auto now = std::chrono::steady_clock::now();
            REQUIRE(now - start < 1s);
        }

        t.join();
    }

    SECTION("IOUringQueue Sleep") {
        auto queue = std::make_unique<IOUringQueue>(10, 10'000);
        std::atomic<DependencyManager*> _dm{};
        Ichor::v1::Detail::_local_dm = nullptr;
        std::thread t([&] {
            REQUIRE(queue->createEventLoop());
            auto &dm = queue->getManager();
            Ichor::v1::Detail::_local_dm = &dm;
            _dm.store(&dm, std::memory_order_release);
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<IOUringSleepService>();
            queue->start(DoNotCaptureSigInt);
        });

        auto start = std::chrono::steady_clock::now();
        while(queue->is_running()) {
            std::this_thread::sleep_for(10ms);
            auto now = std::chrono::steady_clock::now();
            REQUIRE(now - start < 1s);
        }

        t.join();
    }

    SECTION("IOUringQueue more in-flight requests than pooled events") {
        constexpr uint32_t ringSize = 32;
        constexpr uint32_t requests = ringSize * 4;
        auto queue = std::make_unique<TemplateIOUringQueue<ringSize>>(10, 10'000);
        __kernel_timespec ts{};
        ts.tv_nsec = 1'000'000;
        uint32_t completed{};
        uint32_t timedOut{};
        Ichor::v1::Detail::_local_dm = nullptr;
        std::thread t([&] {
            REQUIRE(queue->createEventLoop());
            auto &dm = queue->getManager();
            Ichor::v1::Detail::_local_dm = &dm;
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();

            // all timeouts are in flight at the same time, so the pool of ringSize events is exhausted
            for(uint32_t i = 0; i < requests; ++i) {
                auto *sqe = queue->getSqeWithData(ServiceIdType{0}, [&](io_uring_cqe *cqe) {
                    if(cqe->res == -ETIME) {
                        timedOut++;
                    }
                    if(++completed == requests) {
                        queue->pushEvent<QuitEvent>(ServiceIdType{0});
                    }
                });
                io_uring_prep_timeout(sqe, &ts, 0, 0);
            }
            queue->start(DoNotCaptureSigInt);
        });

        t.join();

        REQUIRE(completed == requests);
        REQUIRE(timedOut == requests);
    }

    SECTION("IOUringQueue re-uses released response events in FIFO order, including the ones allocated when the pool ran out") {
        // FIFO, so that the user_data of a finished request isn't handed out again right away: stale ids (e.g. of an expired timeout)
        // would otherwise target an unrelated new request.
        constexpr uint32_t ringSize = 32;
        constexpr uint32_t requests = ringSize * 2;
        auto queue = std::make_unique<TemplateIOUringQueue<ringSize>>(10, 10'000);
        __kernel_timespec ts{};
        ts.tv_nsec = 1'000'000;
        std::vector<uint64_t> releasedOrder;
        std::vector<uint64_t> reacquiredOrder;
        uint32_t completedSecondRound{};
        Ichor::v1::Detail::_local_dm = nullptr;
        std::thread t([&] {
            REQUIRE(queue->createEventLoop());
            auto &dm = queue->getManager();
            Ichor::v1::Detail::_local_dm = &dm;
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();

            for(uint32_t i = 0; i < requests; ++i) {
                auto *sqe = queue->getSqeWithData(ServiceIdType{0}, [&](io_uring_cqe *cqe) {
                    // released right after this callback, in completion order
                    releasedOrder.push_back(cqe->user_data);
                    if(releasedOrder.size() != requests) {
                        return;
                    }
                    queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
                        for(uint32_t j = 0; j < requests; ++j) {
                            auto *sqe2 = queue->getSqeWithData(ServiceIdType{0}, [&](io_uring_cqe *) {
                                if(++completedSecondRound == requests) {
                                    queue->pushEvent<QuitEvent>(ServiceIdType{0});
                                }
                            });
                            reacquiredOrder.push_back(sqe2->user_data);
                            io_uring_prep_timeout(sqe2, &ts, 0, 0);
                        }
                    });
                });
                io_uring_prep_timeout(sqe, &ts, 0, 0);
            }
            queue->start(DoNotCaptureSigInt);
        });

        t.join();

        REQUIRE(releasedOrder.size() == requests);
        REQUIRE(reacquiredOrder == releasedOrder);
    }

    SECTION("IOUringQueue keeps the order of events pushed from another thread") {
        // Pushes from a thread without an IOUringQueue wait for the delivery of each msg_ring. With DEFER_TASKRUN targets the messages
        // are delivered through task_work, which runs newest-first on kernels 6.2 - 6.10, so not waiting reorders them there.
        // On other kernels this passes either way.
        constexpr uint32_t events = 1'000;
        auto queue = std::make_unique<IOUringQueue>(10, 10'000);
        std::vector<uint32_t> order;
        Ichor::v1::Detail::_local_dm = nullptr;
        std::atomic<DependencyManager*> dmPtr{};
        std::thread t([&] {
            REQUIRE(queue->createEventLoop());
            auto &dm = queue->getManager();
            Ichor::v1::Detail::_local_dm = &dm;
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dmPtr.store(&dm, std::memory_order_release);
            queue->start(DoNotCaptureSigInt);
        });

        while(dmPtr.load(std::memory_order_acquire) == nullptr) {
            std::this_thread::sleep_for(1ms);
        }
        waitForRunning(*dmPtr.load(std::memory_order_acquire));

        for(uint32_t i = 0; i < events; ++i) {
            queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&order, i]() {
                order.push_back(i);
            });
        }
        queue->pushEvent<QuitEvent>(ServiceIdType{0});

        t.join();

        REQUIRE(order.size() == events);
        REQUIRE(std::ranges::is_sorted(order));
    }

    SECTION("IOUringQueue keeps the order of events pushed from another IOUringQueue's thread") {
        // Same as above, from another IOUringQueue's thread, which can't wait for the delivery (two queues pushing to each other would
        // deadlock). Events go through a FIFO, msg_ring only wakes up the target. Only reorders on kernels 6.2 - 6.10 without that.
        constexpr uint32_t events = 1'000;
        // a queue's manager registers itself for the thread constructing it, each queue's thread sets its own below
        auto source = std::make_unique<IOUringQueue>(10, 10'000);
        Ichor::v1::Detail::_local_dm = nullptr;
        auto target = std::make_unique<IOUringQueue>(10, 10'000);
        Ichor::v1::Detail::_local_dm = nullptr;
        std::vector<uint32_t> order;
        std::atomic<DependencyManager*> sourceDm{};
        std::atomic<DependencyManager*> targetDm{};
        std::thread targetThread([&] {
            REQUIRE(target->createEventLoop());
            auto &dm = target->getManager();
            Ichor::v1::Detail::_local_dm = &dm;
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            targetDm.store(&dm, std::memory_order_release);
            target->start(DoNotCaptureSigInt);
        });
        std::thread sourceThread([&] {
            REQUIRE(source->createEventLoop());
            auto &dm = source->getManager();
            Ichor::v1::Detail::_local_dm = &dm;
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            sourceDm.store(&dm, std::memory_order_release);
            source->start(DoNotCaptureSigInt);
        });

        while(sourceDm.load(std::memory_order_acquire) == nullptr || targetDm.load(std::memory_order_acquire) == nullptr) {
            std::this_thread::sleep_for(1ms);
        }
        waitForRunning(*sourceDm.load(std::memory_order_acquire));
        waitForRunning(*targetDm.load(std::memory_order_acquire));

        source->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            for(uint32_t i = 0; i < events; ++i) {
                target->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&order, i]() {
                    order.push_back(i);
                });
            }
            target->pushEvent<QuitEvent>(ServiceIdType{0});
            source->pushEvent<QuitEvent>(ServiceIdType{0});
        });

        targetThread.join();
        sourceThread.join();

        REQUIRE(order.size() == events);
        REQUIRE(std::ranges::is_sorted(order));
    }

    SECTION("IOUringQueue quits while a multishot request keeps completing") {
        auto version = Ichor::v1::kernelVersion();
        REQUIRE(version);
        if(version < Version{.major=6, .minor=4, .patch=0}) {
            SKIP("multishot timeouts require kernel 6.4");
        }
        // a long quit timeout, so that only cancelling the request on quit can make it quit quickly
        auto queue = std::make_unique<IOUringQueue>(10'000, 10'000);
        __kernel_timespec ts{};
        ts.tv_nsec = 50'000;
        std::atomic<uint64_t> completions{};
        Ichor::v1::Detail::_local_dm = nullptr;
        std::atomic<DependencyManager*> dmPtr{};
        std::thread t([&] {
            REQUIRE(queue->createEventLoop());
            auto &dm = queue->getManager();
            Ichor::v1::Detail::_local_dm = &dm;
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            // keeps completing every 50 us, faster than the final loop's wait for no more completions (500 us)
            auto *sqe = queue->getSqeWithData(ServiceIdType{0}, [&completions](io_uring_cqe *) {
                completions++;
            });
            io_uring_prep_timeout(sqe, &ts, 0, IORING_TIMEOUT_MULTISHOT);
            dmPtr.store(&dm, std::memory_order_release);
            queue->start(DoNotCaptureSigInt);
        });

        while(dmPtr.load(std::memory_order_acquire) == nullptr) {
            std::this_thread::sleep_for(1ms);
        }
        waitForRunning(*dmPtr.load(std::memory_order_acquire));
        while(completions.load(std::memory_order_acquire) < 10) {
            std::this_thread::sleep_for(1ms);
        }

        auto const start = std::chrono::steady_clock::now();
        queue->pushEvent<QuitEvent>(ServiceIdType{0});
        while(queue->is_running() || dmPtr.load(std::memory_order_acquire)->isRunning()) {
            std::this_thread::sleep_for(1ms);
            REQUIRE(std::chrono::steady_clock::now() - start < 2s);
        }
        t.join();
        // well below the 10 second quit timeout: the final loop must not wait for the multishot request to stop by itself
        REQUIRE(std::chrono::steady_clock::now() - start < 2s);
    }

    SECTION("IOUringQueue rejects SQPOLL rings") {
        io_uring ring{};
        io_uring_params p{};
        p.flags = IORING_SETUP_SQPOLL;
        p.sq_thread_idle = 10;
        // IOUringQueue checks the ring size first, so use the size it expects
        auto ret = io_uring_queue_init_params(2048, &ring, &p);
        if(ret < 0) {
            SKIP("Can't create an SQPOLL ring: " << -ret);
        }
        ScopeGuard sg{[&ring]() {
            io_uring_queue_exit(&ring);
        }};

        auto queue = std::make_unique<IOUringQueue>();
        // the queue assumes sqes are consumed when submitting, a kernel thread consumes them asynchronously with SQPOLL
        REQUIRE(!queue->useEventLoop(&ring));
    }

    SECTION("IOUringQueue destroys events pushed while handling its last batch") {
        CountedEvent::created = 0;
        CountedEvent::destroyed = 0;
        auto queue = std::make_unique<IOUringQueue>(10, 10'000);
        Ichor::v1::Detail::_local_dm = nullptr;
        std::atomic<DependencyManager*> dmPtr{};
        std::thread t([&] {
            REQUIRE(queue->createEventLoop());
            auto &dm = queue->getManager();
            Ichor::v1::Detail::_local_dm = &dm;
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<PushesEventOnStopService>();
            dmPtr.store(&dm, std::memory_order_release);
            queue->start(DoNotCaptureSigInt);
        });

        while(dmPtr.load(std::memory_order_acquire) == nullptr) {
            std::this_thread::sleep_for(1ms);
        }
        waitForRunning(*dmPtr.load(std::memory_order_acquire));
        queue->pushEvent<QuitEvent>(ServiceIdType{0});

        t.join();
        queue.reset();

        REQUIRE(CountedEvent::created >= 1);
        REQUIRE(CountedEvent::destroyed == CountedEvent::created);
    }

#endif
}
