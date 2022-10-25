#include "Common.h"
#include "TestEvents.h"
#include "TestServices/UselessService.h"
#include <ichor/event_queues/MultimapQueue.h>
#ifdef ICHOR_USE_SDEVENT
#include <ichor/event_queues/SdeventQueue.h>
#endif

TEST_CASE("QueueTests") {

    SECTION("MultimapQueue") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        REQUIRE_THROWS(queue->pushEvent(0, nullptr));

        REQUIRE(queue->empty());
        REQUIRE(queue->size() == 0);
        REQUIRE(!queue->shouldQuit());

        REQUIRE_NOTHROW(queue->pushEvent(10, std::make_unique<TestEvent>(0, 0, 10)));

        REQUIRE(!queue->empty());
        REQUIRE(queue->size() == 1);

        queue->quit();

        REQUIRE(!queue->empty());
        REQUIRE(queue->size() == 1);
        REQUIRE(queue->shouldQuit());
    }

#ifdef ICHOR_USE_SDEVENT
    SECTION("SdeventQueue") {
        auto queue = std::make_unique<SdeventQueue>();
        auto &dm = queue->createManager();

        REQUIRE_THROWS(queue->pushEvent(0, nullptr));
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

        std::atomic<DependencyManager*> _dm{nullptr};
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

        while(_dm == nullptr) {
            std::this_thread::sleep_for(1ms);
        }

        _dm.load(std::memory_order_acquire)->runForOrQueueEmpty();

        REQUIRE(_dm.load(std::memory_order_acquire)->isRunning());

        try {
            _dm.load(std::memory_order_acquire)->pushEvent<QuitEvent>(0);
        } catch(const std::exception &e) {
            fmt::print("exception: {}\n", e.what());
            throw;
        }

        t.join();
    }
#endif
}