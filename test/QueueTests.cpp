#include "Common.h"
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

TEST_CASE("QueueTests") {

    SECTION("PriorityQueue") {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();

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
        auto &dm = queue->createManager();

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

    SECTION("Delete of uninitialized queues") {
        auto f = []() {
            std::array<PriorityQueue, 8> queues{};
        };
#if ICHOR_EXCEPTIONS_ENABLED
        REQUIRE_NOTHROW(f());
#else
        f();
#endif
    }

#ifdef ICHOR_USE_SDEVENT
    SECTION("SdeventQueue") {
        auto queue = std::make_unique<SdeventQueue>();
        auto &dm = queue->createManager();

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
            auto &dm = queue->createManager();
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
        auto &dm = queue->createManager();

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
            auto &dm = queue->createManager();
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
        auto &dm = queue->createManager();

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
        std::thread t([&] {
            REQUIRE(queue->createEventLoop());
            auto &dm = queue->createManager();
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
        std::thread t([&] {
            REQUIRE(queue->createEventLoop());
            auto &dm = queue->createManager();
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

#endif
}
