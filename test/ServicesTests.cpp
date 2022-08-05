#include "Common.h"
#include "TestServices/UselessService.h"
#include "TestServices/QuitOnStartWithDependenciesService.h"
#include "TestServices/DependencyService.h"
#include "TestServices/MixingInterfacesService.h"
#include "TestServices/StartStopOnSecondAttemptService.h"

TEST_CASE("DependencyServices") {

    ensureInternalLoggerExists();

    SECTION("QuitOnQuitEvent") {
        Ichor::DependencyManager dm{};

        std::thread t([&]() {
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService>();
            dm.createServiceManager<QuitOnStartWithDependenciesService>();
            dm.start();
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Required dependencies") {
        Ichor::DependencyManager dm{};
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
            dm.start();
        });

        waitForRunning(dm);

        dm.waitForEmptyQueue();

        dm.pushEvent<RunFunctionEvent>(0, [secondUselessServiceId](DependencyManager* mng){
            auto services = mng->getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 2);

            mng->pushEvent<StopServiceEvent>(0, secondUselessServiceId);
        });

        dm.waitForEmptyQueue();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager* mng){
            auto services = mng->getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 1);

            mng->pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Optional dependencies") {
        Ichor::DependencyManager dm{};
        uint64_t secondUselessServiceId{};

        std::thread t([&]() {
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>({}, 10);
            dm.createServiceManager<UselessService, IUselessService>();
            secondUselessServiceId = dm.createServiceManager<UselessService, IUselessService>()->getServiceId();
            dm.createServiceManager<DependencyService<false>, ICountService>();
            dm.start();
        });

        waitForRunning(dm);

        dm.waitForEmptyQueue();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager* mng){
            auto services = mng->getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);

            REQUIRE(services[0]->getSvcCount() == 2);
        });

        dm.waitForEmptyQueue();

        dm.pushEvent<StopServiceEvent>(0, secondUselessServiceId);

        dm.waitForEmptyQueue();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager* mng){
            auto services = mng->getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);

            REQUIRE(services[0]->getSvcCount() == 1);

            mng->pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Mixing services should not cause UB") {
        Ichor::DependencyManager dm{};

        std::thread t([&]() {
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<MixServiceOne, IMixOne, IMixTwo>();
            dm.createServiceManager<MixServiceTwo, IMixOne, IMixTwo>();
            dm.createServiceManager<MixServiceThree, IMixOne, IMixTwo>();
            dm.createServiceManager<MixServiceFour, IMixOne, IMixTwo>();
            dm.createServiceManager<MixServiceFive, IMixOne, IMixTwo>();
            dm.createServiceManager<MixServiceSix, IMixOne, IMixTwo>();
            dm.createServiceManager<CheckMixService, ICountService>();
            dm.start();
        });

        t.join();
    }

    SECTION("Dependency manager should retry failed starts/stops") {
        Ichor::DependencyManager dm{};
        StartStopOnSecondAttemptService *svc;

        std::thread t([&]() {
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
            svc = dm.createServiceManager<StartStopOnSecondAttemptService>();
            dm.start();
        });

        waitForRunning(dm);

        dm.waitForEmptyQueue();

        dm.pushEvent<RunFunctionEvent>(0, [&](DependencyManager* mng){
            REQUIRE(svc->starts == 2);

            mng->pushEvent<StopServiceEvent>(0, svc->getServiceId());
        });

        dm.waitForEmptyQueue();

        dm.pushEvent<RunFunctionEvent>(0, [&](DependencyManager* mng){
            REQUIRE(svc->stops == 2);

            mng->pushEvent<QuitEvent>(0);
        });

        t.join();
    }
}