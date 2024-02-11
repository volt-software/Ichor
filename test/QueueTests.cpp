#include "Common.h"
#include "TestEvents.h"
#include "TestServices/UselessService.h"
#include <ichor/event_queues/PriorityQueue.h>
//#include <spdlog/spdlog.h>
//#include <spdlog/sinks/stdout_color_sinks.h>
#ifdef ICHOR_USE_SDEVENT
#include <ichor/event_queues/SdeventQueue.h>
#endif
#ifdef ICHOR_USE_LIBURING
#include <ichor/event_queues/IOUringQueue.h>
#endif

TEST_CASE("QueueTests") {

    SECTION("PriorityQueue") {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();

        REQUIRE_THROWS(queue->pushEventInternal(0, nullptr));

        REQUIRE(queue->empty());
        REQUIRE(queue->size() == 0);
        REQUIRE(!queue->shouldQuit());

        REQUIRE_NOTHROW(queue->pushEventInternal(10, std::make_unique<TestEvent>(0, 0, 10)));

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

        REQUIRE_THROWS(queue->pushEventInternal(0, nullptr));

        REQUIRE(queue->empty());
        REQUIRE(queue->size() == 0);
        REQUIRE(!queue->shouldQuit());

        REQUIRE_NOTHROW(queue->pushEventInternal(10, std::make_unique<TestEvent>(0, 0, 10)));

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
        REQUIRE_NOTHROW(f());
    }

#ifdef ICHOR_USE_SDEVENT
    SECTION("SdeventQueue") {
        auto queue = std::make_unique<SdeventQueue>();
        auto &dm = queue->createManager();

        Detail::_local_dm = &dm;

        REQUIRE_THROWS(queue->pushEventInternal(0, nullptr));
        REQUIRE_THROWS(queue->empty());
        REQUIRE_THROWS(queue->size());

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

        try {
            dm->getEventQueue().pushEvent<QuitEvent>(0);
        } catch(const std::exception &e) {
            fmt::print("exception: {}\n", e.what());
            throw;
        }

        t.join();
    }
#endif

#ifdef ICHOR_USE_LIBURING
    SECTION("IOUringQueue") {
        auto queue = std::make_unique<IOUringQueue>();
        auto &dm = queue->createManager();

        Detail::_local_dm = &dm;

        REQUIRE_THROWS(queue->pushEventInternal(0, nullptr));
        REQUIRE_THROWS(queue->empty());
        REQUIRE_THROWS(queue->size());

        auto loop = queue->createEventLoop();

        REQUIRE(queue->empty());
        REQUIRE(queue->size() == 0);
        REQUIRE(!queue->shouldQuit());
    }

    SECTION("IOUringQueue Live") {
//        auto new_logger = spdlog::stdout_color_st("default_logger");
//        spdlog::set_default_logger(new_logger);
        std::atomic<DependencyManager*> _dm{};
        std::thread t([&] {
            auto queue = std::make_unique<IOUringQueue>(10, 10'000);
            auto &dm = queue->createManager();
            _dm.store(&dm, std::memory_order_release);

            queue->createEventLoop();
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService>();
            queue->start(DoNotCaptureSigInt);
        });

//        spdlog::info("1");

        while(_dm.load(std::memory_order_acquire) == nullptr) {
            std::this_thread::sleep_for(1ms);
        }
        auto *dm = _dm.load(std::memory_order_acquire);

//        spdlog::info("2");

        dm->runForOrQueueEmpty();

//        spdlog::info("3");

        REQUIRE(dm->isRunning());

        try {
            dm->getEventQueue().pushEvent<QuitEvent>(0);
        } catch(const std::exception &e) {
            fmt::print("exception: {}\n", e.what());
            throw;
        }

//        spdlog::info("4");

        t.join();

//        spdlog::info("5");
    }
#endif
}
