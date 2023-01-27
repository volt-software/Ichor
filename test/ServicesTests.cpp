#include "Common.h"
#include "TestServices/FailOnStartService.h"
#include "TestServices/UselessService.h"
#include "TestServices/QuitOnStartWithDependenciesService.h"
#include "TestServices/DependencyService.h"
#include "TestServices/MixingInterfacesService.h"
#include "TestServices/TimerRunsOnceService.h"
#include "TestServices/AddEventHandlerDuringEventHandlingService.h"
#include "TestServices/RequestsLoggingService.h"
#include "TestServices/ConstructorInjectionTestService.h"
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/services/logging/LoggerAdmin.h>
#include <ichor/services/logging/CoutLogger.h>
#include <ichor/dependency_management/ConstructorInjectionService.h>

bool AddEventHandlerDuringEventHandlingService::_addedReg{};

TEST_CASE("ServicesTests") {

    SECTION("QuitOnQuitEvent") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService, IUselessService>();
            dm.createServiceManager<QuitOnStartWithDependenciesService>();
            queue->start(CaptureSigInt);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("FailOnStartService") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<FailOnStartService, IFailOnStartService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager& mng) {
            auto services = mng.getStartedServices<IFailOnStartService>();

            REQUIRE(services.empty());

            std::vector<IFailOnStartService*> svcs = mng.getAllServicesOfType<IFailOnStartService>();

            REQUIRE(svcs.size() == 1);
            REQUIRE(svcs[0]->getStartCount() == 1);

            mng.pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("FailOnStartWithDependenciesService") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService, IUselessService>();
            dm.createServiceManager<FailOnStartWithDependenciesService, IFailOnStartService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager& mng) {
            auto services = mng.getStartedServices<IFailOnStartService>();

            REQUIRE(services.empty());

            std::vector<IFailOnStartService*> svcs = mng.getAllServicesOfType<IFailOnStartService>();

            REQUIRE(svcs.size() == 1);
            REQUIRE(svcs[0]->getStartCount() == 1);

            mng.pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Required dependencies") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        uint64_t secondUselessServiceId{};

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService, IUselessService>();
            secondUselessServiceId = dm.createServiceManager<UselessService, IUselessService>()->getServiceId();
            dm.createServiceManager<DependencyService<true>, ICountService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [secondUselessServiceId](DependencyManager& mng) {
            auto services = mng.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 2);

            mng.pushEvent<StopServiceEvent>(0, secondUselessServiceId);
        });

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager& mng) {
            auto services = mng.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 1);

            mng.pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Optional dependencies") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        uint64_t secondUselessServiceId{};

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService, IUselessService>();
            secondUselessServiceId = dm.createServiceManager<UselessService, IUselessService>()->getServiceId();
            dm.createServiceManager<DependencyService<false>, ICountService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager& mng) {
            auto services = mng.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);

            REQUIRE(services[0]->getSvcCount() == 2);
        });

        dm.runForOrQueueEmpty();

        dm.pushEvent<StopServiceEvent>(0, secondUselessServiceId);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager& mng) {
            auto services = mng.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);

            REQUIRE(services[0]->getSvcCount() == 1);

            mng.pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Mixing services should not cause UB") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
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

    SECTION("TimerService runs exactly once") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        TimerRunsOnceService *svc{};

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            svc = dm.createServiceManager<TimerRunsOnceService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [&](DependencyManager& mng) {
            REQUIRE(svc->count == 1);

            mng.pushEvent<QuitEvent>(svc->getServiceId());
        });

        t.join();
    }

    SECTION("Add event handler during event handling") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<AddEventHandlerDuringEventHandlingService>();
            dm.createServiceManager<AddEventHandlerDuringEventHandlingService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<TestEvent>(0);

        dm.runForOrQueueEmpty();

        dm.pushEvent<QuitEvent>(0);

        t.join();
    }

    SECTION("LoggerAdmin removes logger when service is gone") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        uint64_t svcId{};

        std::thread t([&]() {
            dm.createServiceManager<LoggerAdmin<CoutLogger>, ILoggerAdmin>();
            svcId = dm.createServiceManager<RequestsLoggingService, IRequestsLoggingService>()->getServiceId();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [&](DependencyManager& mng) {
            REQUIRE(mng.getServiceCount() == 3);

            mng.pushEvent<StopServiceEvent>(0, svcId);
            // + 11 because the first stop triggers a dep offline event and inserts a new stop with 10 higher priority.
            mng.pushPrioritisedEvent<RemoveServiceEvent>(0, INTERNAL_EVENT_PRIORITY + 11, svcId);
        });

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [&](DependencyManager& mng) {
            REQUIRE(mng.getServiceCount() == 1);

            mng.pushEvent<QuitEvent>(0);
        });

        t.join();
    }

    SECTION("ConstructorInjectionService basic test") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        uint64_t svcId{};

        std::thread t([&]() {
            dm.createServiceManager<LoggerAdmin<CoutLogger>, ILoggerAdmin>();
            dm.createServiceManager<DependencyService<false>, ICountService>();
            svcId = dm.createServiceManager<ConstructorInjectionTestService>()->getServiceId();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [&](DependencyManager& mng) {
            REQUIRE(mng.getServiceCount() == 4);

            mng.pushEvent<StopServiceEvent>(0, svcId);
            // + 11 because the first stop triggers a dep offline event and inserts a new stop with 10 higher priority.
            mng.pushPrioritisedEvent<RemoveServiceEvent>(0, INTERNAL_EVENT_PRIORITY + 11, svcId);
        });

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [&](DependencyManager& mng) {
            REQUIRE(mng.getServiceCount() == 2);

            mng.pushEvent<QuitEvent>(0);
        });

        t.join();
    }
}
