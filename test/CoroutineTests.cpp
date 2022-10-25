#include "Common.h"
#include "TestServices/GeneratorService.h"
#include "TestServices/AwaitService.h"
#include "TestServices/MultipleAwaitersService.h"
#include "TestServices/AsyncUsingTimerService.h"
#include "TestServices/AwaitReturnService.h"
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/events/RunFunctionEvent.h>
#include <memory>

std::unique_ptr<Ichor::AsyncManualResetEvent> _evt;
std::unique_ptr<Ichor::AsyncAutoResetEvent> _autoEvt;
uint64_t AwaitNoCopy::countConstructed{};
uint64_t AwaitNoCopy::countDestructed{};
uint64_t AwaitNoCopy::countMoved{};

TEST_CASE("CoroutineTests") {

    SECTION("Required dependencies") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        _evt = std::make_unique<Ichor::AsyncManualResetEvent>();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<GeneratorService, IGeneratorService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager& mng) -> AsyncGenerator<void>{
            auto services = mng.getStartedServices<IGeneratorService>();

            REQUIRE(services.size() == 1);

            auto gen = services[0]->infinite_int();
            INTERNAL_DEBUG("infinite_int");
            auto it = gen.begin();
            INTERNAL_DEBUG("it");
            REQUIRE(*it.await_resume() == 0);
            INTERNAL_DEBUG("resume");
            it = gen.begin();
            INTERNAL_DEBUG("it2");
            REQUIRE(*it.await_resume() == 1);
            INTERNAL_DEBUG("resume2");
            it = gen.begin();
            INTERNAL_DEBUG("it3");
            REQUIRE(*it.await_resume() == 2);
            INTERNAL_DEBUG("resume3");

            mng.pushEvent<QuitEvent>(0);
            INTERNAL_DEBUG("quit");

            co_return;
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("co_await in function") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        _evt = std::make_unique<Ichor::AsyncManualResetEvent>();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<AwaitService, IAwaitService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager& mng) -> AsyncGenerator<void> {
            auto services = mng.getStartedServices<IAwaitService>();

            REQUIRE(services.size() == 1);

            INTERNAL_DEBUG("before");

            co_await services[0]->await_something().begin();

            INTERNAL_DEBUG("after");

            co_yield empty;

            INTERNAL_DEBUG("quit");

            mng.pushEvent<QuitEvent>(0);

            INTERNAL_DEBUG("after2");

            co_return;
        });

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager& mng) -> AsyncGenerator<void> {
            INTERNAL_DEBUG("set");
            _evt->set();
            co_return;
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("co_await in event handler") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        _evt = std::make_unique<Ichor::AsyncManualResetEvent>();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<EventAwaitService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<AwaitEvent>(0);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager& mng) -> AsyncGenerator<void> {
            INTERNAL_DEBUG("set");
            _evt->set();
            co_return;
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("multiple awaiters in event handler") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        _autoEvt = std::make_unique<Ichor::AsyncAutoResetEvent>();
        MultipleAwaitService *svc{};

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            svc = dm.createServiceManager<MultipleAwaitService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [&](DependencyManager& mng) -> AsyncGenerator<void> {
            INTERNAL_DEBUG("set");
            _autoEvt->set_all();
            REQUIRE(svc->count == 2);
            mng.pushEvent<QuitEvent>(0);
            co_return;
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("co_await in timer service") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        _evt = std::make_unique<Ichor::AsyncManualResetEvent>();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<AwaitService, IAwaitService>();
            dm.createServiceManager<AsyncUsingTimerService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager& mng) -> AsyncGenerator<void> {
            INTERNAL_DEBUG("set");
            _evt->set();
            co_return;
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("co_await user defined struct") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        _evt = std::make_unique<Ichor::AsyncManualResetEvent>();
        AwaitReturnService *svc{};

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            svc = dm.createServiceManager<AwaitReturnService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [&](DependencyManager& mng) -> AsyncGenerator<void> {
            auto &str = *co_await svc->Await().begin();
            co_return;
        });

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [&](DependencyManager& mng) -> AsyncGenerator<void> {
            INTERNAL_DEBUG("set");
            _evt->set();
            mng.pushEvent<QuitEvent>(0);
            co_return;
        });

        t.join();

        REQUIRE(AwaitNoCopy::countConstructed == 1);
        REQUIRE(AwaitNoCopy::countDestructed == 2);
        REQUIRE(AwaitNoCopy::countMoved == 1);

        REQUIRE_FALSE(dm.isRunning());
    }

    _evt.reset();
}