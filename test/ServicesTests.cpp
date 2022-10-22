#include "Common.h"
#include "TestServices/UselessService.h"
#include "TestServices/QuitOnStartWithDependenciesService.h"
#include "TestServices/DependencyService.h"
#include "TestServices/MixingInterfacesService.h"
#include "TestServices/StartStopOnSecondAttemptService.h"
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/events/RunFunctionEvent.h>

TEST_CASE("ServicesTests") {

    ensureInternalLoggerExists();

    SECTION("QuitOnQuitEvent") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
#if __has_include(<spdlog/spdlog.h>)
            dm.createServiceManager<SpdlogFrameworkLogger, IFrameworkLogger>();
#else
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
#endif
            dm.createServiceManager<UselessService>();
            dm.createServiceManager<QuitOnStartWithDependenciesService>();
            queue->start(CaptureSigInt);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Required dependencies") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        uint64_t secondUselessServiceId{};

        std::thread t([&]() {
#if __has_include(<spdlog/spdlog.h>)
            dm.createServiceManager<SpdlogFrameworkLogger, IFrameworkLogger>();
#else
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
#endif
            dm.createServiceManager<UselessService, IUselessService>();
            secondUselessServiceId = dm.createServiceManager<UselessService, IUselessService>()->getServiceId();
            dm.createServiceManager<DependencyService<true>, ICountService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [secondUselessServiceId](DependencyManager* mng) -> AsyncGenerator<bool> {
            auto services = mng->getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 2);

            mng->pushEvent<StopServiceEvent>(0, secondUselessServiceId);

            co_return (bool)PreventOthersHandling;
        });

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager* mng) -> AsyncGenerator<bool> {
            auto services = mng->getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 1);

            mng->pushEvent<QuitEvent>(0);

            co_return (bool)PreventOthersHandling;
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Optional dependencies") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        uint64_t secondUselessServiceId{};

        std::thread t([&]() {
#if __has_include(<spdlog/spdlog.h>)
            dm.createServiceManager<SpdlogFrameworkLogger, IFrameworkLogger>();
#else
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
#endif
            dm.createServiceManager<UselessService, IUselessService>();
            secondUselessServiceId = dm.createServiceManager<UselessService, IUselessService>()->getServiceId();
            dm.createServiceManager<DependencyService<false>, ICountService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager* mng) -> AsyncGenerator<bool> {
            auto services = mng->getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);

            REQUIRE(services[0]->getSvcCount() == 2);

            co_return (bool)PreventOthersHandling;
        });

        dm.runForOrQueueEmpty();

        dm.pushEvent<StopServiceEvent>(0, secondUselessServiceId);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager* mng) -> AsyncGenerator<bool> {
            auto services = mng->getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);

            REQUIRE(services[0]->getSvcCount() == 1);

            mng->pushEvent<QuitEvent>(0);

            co_return (bool)PreventOthersHandling;
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Mixing services should not cause UB") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<MixServiceOne, IMixOne, IMixTwo>();
            dm.createServiceManager<MixServiceTwo, IMixOne, IMixTwo>();
            dm.createServiceManager<MixServiceThree, IMixOne, IMixTwo>();
            dm.createServiceManager<MixServiceFour, IMixOne, IMixTwo>();
            dm.createServiceManager<MixServiceFive, IMixOne, IMixTwo>();
            dm.createServiceManager<MixServiceSix, IMixOne, IMixTwo>();
            dm.createServiceManager<CheckMixService, ICountService>();
            queue->start(CaptureSigInt);
        });

        t.join();
    }

    SECTION("Dependency manager should retry failed starts/stops") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        StartStopOnSecondAttemptService *svc{};

        std::thread t([&]() {
#if __has_include(<spdlog/spdlog.h>)
            dm.createServiceManager<SpdlogFrameworkLogger, IFrameworkLogger>();
#else
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
#endif
            svc = dm.createServiceManager<StartStopOnSecondAttemptService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [&](DependencyManager* mng) -> AsyncGenerator<bool> {
            REQUIRE(svc->starts == 2);

            mng->pushEvent<StopServiceEvent>(0, svc->getServiceId());

            co_return (bool)PreventOthersHandling;
        });

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [&](DependencyManager* mng) -> AsyncGenerator<bool> {
            REQUIRE(svc->stops == 2);

            mng->pushEvent<QuitEvent>(0);

            co_return (bool)PreventOthersHandling;
        });

        t.join();
    }
}