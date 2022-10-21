#include "Common.h"
#include "TestServices/GeneratorService.h"
#include "TestServices/AwaitService.h"
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/events/RunFunctionEvent.h>
#include <memory>

#if __has_include(<spdlog/spdlog.h>)
#include <ichor/optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#else
#include <ichor/optional_bundles/logging_bundle/NullFrameworkLogger.h>
#endif

std::unique_ptr<Ichor::AsyncManualResetEvent> _evt;

TEST_CASE("CoroutineTests") {

    ensureInternalLoggerExists();

    SECTION("Required dependencies") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        _evt = std::make_unique<Ichor::AsyncManualResetEvent>();

        std::thread t([&]() {

#if __has_include(<spdlog/spdlog.h>)
            auto logger = dm.createServiceManager<SpdlogFrameworkLogger, IFrameworkLogger>({}, 10);
            logger->setLogLevel(Ichor::LogLevel::DEBUG);
#else
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
#endif
            dm.createServiceManager<GeneratorService, IGeneratorService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager* mng) -> AsyncGenerator<bool>{
            auto services = mng->getStartedServices<IGeneratorService>();

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

            mng->pushEvent<QuitEvent>(0);
            INTERNAL_DEBUG("quit");

            co_return (bool)PreventOthersHandling;
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("co_await in function") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        _evt = std::make_unique<Ichor::AsyncManualResetEvent>();

        std::thread t([&]() {
#if __has_include(<spdlog/spdlog.h>)
            auto logger = dm.createServiceManager<SpdlogFrameworkLogger, IFrameworkLogger>({}, 10);
            logger->setLogLevel(Ichor::LogLevel::DEBUG);
#else
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
#endif
            dm.createServiceManager<AwaitService, IAwaitService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager* mng) -> AsyncGenerator<bool> {
            auto services = mng->getStartedServices<IAwaitService>();

            REQUIRE(services.size() == 1);

            INTERNAL_DEBUG("before");

            co_await services[0]->await_something().begin();

            INTERNAL_DEBUG("after");

            co_yield (bool)PreventOthersHandling;

            INTERNAL_DEBUG("quit");

            mng->pushEvent<QuitEvent>(0);

            INTERNAL_DEBUG("after2");

            co_return (bool)PreventOthersHandling;
        });

        std::this_thread::sleep_for(5ms);

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager* mng) -> AsyncGenerator<bool> {
            INTERNAL_DEBUG("set");
            _evt->set();
            co_return (bool)PreventOthersHandling;
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("co_await in event handler") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        _evt = std::make_unique<Ichor::AsyncManualResetEvent>();

        std::thread t([&]() {
#if __has_include(<spdlog/spdlog.h>)
            auto logger = dm.createServiceManager<SpdlogFrameworkLogger, IFrameworkLogger>({}, 10);
            logger->setLogLevel(Ichor::LogLevel::DEBUG);
#else
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
#endif
            dm.createServiceManager<EventAwaitService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<AwaitEvent>(0);

        std::this_thread::sleep_for(5ms);

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager* mng) -> AsyncGenerator<bool> {
            INTERNAL_DEBUG("set");
            _evt->set();
            co_return (bool)PreventOthersHandling;
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    _evt.reset();
}