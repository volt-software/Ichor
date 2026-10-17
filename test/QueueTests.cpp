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
#include <vector>
#include <algorithm>

namespace {
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

        Ichor::Detail::_local_dm = &dm;

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

        Ichor::Detail::_local_dm = &dm;

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

        Ichor::Detail::_local_dm = &dm;

//        REQUIRE_THROWS(queue->pushEventInternal(0, nullptr));
//        REQUIRE_THROWS(queue->empty());
//        REQUIRE_THROWS(queue->size());

        REQUIRE(queue->createEventLoop());
        REQUIRE(queue->empty());
        REQUIRE(queue->size() == 0);
        REQUIRE(!queue->shouldQuit());
    }

    SECTION("IOUringQueue Live") {
        auto version = Ichor::v1::kernelVersion();

        REQUIRE(version);
        if(version < Version{5, 18, 0}) {
            return;
        }

        auto queue = std::make_unique<IOUringQueue>(10, 10'000);
        std::atomic<DependencyManager*> _dm{};
        Ichor::Detail::_local_dm = nullptr;
        std::thread t([&] {
            REQUIRE(queue->createEventLoop());
            auto &dm = queue->getManager();
            Ichor::Detail::_local_dm = &dm;
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
        Ichor::Detail::_local_dm = nullptr;
        std::thread t([&] {
            REQUIRE(queue->createEventLoop());
            auto &dm = queue->getManager();
            Ichor::Detail::_local_dm = &dm;
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
        Ichor::Detail::_local_dm = nullptr;
        std::thread t([&] {
            REQUIRE(queue->createEventLoop());
            auto &dm = queue->getManager();
            Ichor::Detail::_local_dm = &dm;
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
        Ichor::Detail::_local_dm = nullptr;
        std::thread t([&] {
            REQUIRE(queue->createEventLoop());
            auto &dm = queue->getManager();
            Ichor::Detail::_local_dm = &dm;
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
        Ichor::Detail::_local_dm = nullptr;
        std::atomic<DependencyManager*> dmPtr{};
        std::thread t([&] {
            REQUIRE(queue->createEventLoop());
            auto &dm = queue->getManager();
            Ichor::Detail::_local_dm = &dm;
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
        REQUIRE(std::is_sorted(order.begin(), order.end()));
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
        Ichor::Detail::_local_dm = nullptr;
        std::atomic<DependencyManager*> dmPtr{};
        std::thread t([&] {
            REQUIRE(queue->createEventLoop());
            auto &dm = queue->getManager();
            Ichor::Detail::_local_dm = &dm;
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
