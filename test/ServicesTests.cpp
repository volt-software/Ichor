#include "catch2/catch_test_macros.hpp"
#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/NullFrameworkLogger.h>
#include "UselessService.h"
#include "QuitOnStartWithDependenciesService.h"
#include "DependencyService.h"
#include "MixingInterfacesService.h"
#include "StartStopOnSecondAttemptService.h"
#include "spdlog/sinks/stdout_color_sinks.h"

TEST_CASE("DependencyServices") {

#if __has_include(<spdlog/spdlog.h>)
    //default logger is disabled in cmake
    if(spdlog::default_logger_raw() == nullptr) {
        auto new_logger = spdlog::stdout_color_st("new_default_logger");
        spdlog::set_default_logger(new_logger);
    }
#endif

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

        std::thread t([&]() {
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService, IUselessService>();
            dm.createServiceManager<DependencyService<true>, ICountService>();
            dm.start();
        });

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

    SECTION("Optional dependencies") {
        Ichor::DependencyManager dm{};
        uint64_t secondUselessServiceId{};

        std::thread t([&]() {
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>({}, 10);
            dm.createServiceManager<UselessService, IUselessService>();
            auto secondSvc = dm.createServiceManager<UselessService, IUselessService>();
            secondUselessServiceId = secondSvc->getServiceId();
            dm.createServiceManager<DependencyService<false>, ICountService>();
            dm.start();
        });

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