#include "Common.h"
#include "TestServices/GeneratorService.h"
#include <ichor/event_queues/MultimapQueue.h>

TEST_CASE("DependencyServices") {

    ensureInternalLoggerExists();

    SECTION("Required dependencies") {
        Ichor::DependencyManager dm{std::make_unique<MultimapQueue>()};

        std::thread t([&]() {
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<GeneratorService, IGeneratorService>();
            dm.start();
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager* mng){
            auto services = mng->getStartedServices<IGeneratorService>();

            REQUIRE(services.size() == 1);

            auto gen = services[0]->infinite_int();
            auto it = gen.begin();
            REQUIRE(*it == 0);
            it++;
            REQUIRE(*it == 1);
            it++;
            REQUIRE(*it == 2);

            mng->pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }
}