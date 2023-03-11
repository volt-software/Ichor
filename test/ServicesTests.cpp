#include "Common.h"
#include "TestServices/FailOnStartService.h"
#include "TestServices/UselessService.h"
#include "TestServices/QuitOnStartWithDependenciesService.h"
#include "TestServices/DependencyService.h"
#include "TestServices/MixingInterfacesService.h"
#include "TestServices/TimerRunsOnceService.h"
#include "TestServices/AddEventHandlerDuringEventHandlingService.h"
#include "TestServices/RequestsLoggingService.h"
#include "TestServices/ConstructorInjectionTestServices.h"
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/logging/CoutLogger.h>
#include <ichor/services/timer/TimerFactoryFactory.h>

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

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            auto services = dm.getStartedServices<IFailOnStartService>();

            REQUIRE(services.empty());

            auto svcs = dm.getAllServicesOfType<IFailOnStartService>();

            REQUIRE(svcs.size() == 1);
            REQUIRE(svcs[0].first.getStartCount() == 1);

            dm.getEventQueue().pushEvent<QuitEvent>(0);
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

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            auto services = dm.getStartedServices<IFailOnStartService>();

            REQUIRE(services.empty());

            auto svcs = dm.getAllServicesOfType<IFailOnStartService>();

            REQUIRE(svcs.size() == 1);
            REQUIRE(svcs[0].first.getStartCount() == 1);

            dm.getEventQueue().pushEvent<QuitEvent>(0);
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

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 2);

            dm.getEventQueue().pushEvent<StopServiceEvent>(0, secondUselessServiceId);
        });

        dm.runForOrQueueEmpty();

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 1);

            dm.getEventQueue().pushEvent<QuitEvent>(0);
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

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);

            REQUIRE(services[0]->getSvcCount() == 2);
        });

        dm.runForOrQueueEmpty();

        queue->pushEvent<StopServiceEvent>(0, secondUselessServiceId);

        dm.runForOrQueueEmpty();

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);

            REQUIRE(services[0]->getSvcCount() == 1);

            dm.getEventQueue().pushEvent<QuitEvent>(0);
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
        uint64_t svcId{};

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            svcId = dm.createServiceManager<TimerRunsOnceService, ITimerRunsOnceService>()->getServiceId();
            dm.createServiceManager<TimerFactoryFactory>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            auto ret = dm.getService<ITimerRunsOnceService>(svcId);
            REQUIRE(ret->first->getCount() == 1);

            dm.getEventQueue().pushEvent<QuitEvent>(svcId);
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

        queue->pushEvent<TestEvent>(0);

        dm.runForOrQueueEmpty();

        queue->pushEvent<QuitEvent>(0);

        t.join();
    }

    SECTION("LoggerAdmin removes logger when service is gone") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        uint64_t svcId{};

        std::thread t([&]() {
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>();
            svcId = dm.createServiceManager<RequestsLoggingService, IRequestsLoggingService>()->getServiceId();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            REQUIRE(dm.getServiceCount() == 5);

            dm.getEventQueue().pushEvent<StopServiceEvent>(0, svcId);
            // + 11 because the first stop triggers a dep offline event and inserts a new stop with 10 higher priority.
            dm.getEventQueue().pushPrioritisedEvent<RemoveServiceEvent>(0, INTERNAL_EVENT_PRIORITY + 11, svcId);
        });

        dm.runForOrQueueEmpty();

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            REQUIRE(dm.getServiceCount() == 3);

            dm.getEventQueue().pushEvent<QuitEvent>(0);
        });

        t.join();
    }

    SECTION("ConstructorInjectionService basic test") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        uint64_t svcId{};

        std::thread t([&]() {
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>();
            dm.createServiceManager<DependencyService<false>, ICountService>();
            auto *service = dm.createServiceManager<ConstructorInjectionTestService, IConstructorInjectionTestService>();
            svcId = service->getServiceId();
            static_assert(std::is_same_v<decltype(service), IService*>, "");
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            REQUIRE(dm.getServiceCount() == 6);
            auto svcs = dm.getAllServicesOfType<IConstructorInjectionTestService>();
            REQUIRE(svcs.size() == 1);
            REQUIRE(svcs[0].second.getServiceId() == svcId);

            dm.getEventQueue().pushEvent<StopServiceEvent>(0, svcId);
            // + 11 because the first stop triggers a dep offline event and inserts a new stop with 10 higher priority.
            dm.getEventQueue().pushPrioritisedEvent<RemoveServiceEvent>(0, INTERNAL_EVENT_PRIORITY + 11, svcId);
        });

        dm.runForOrQueueEmpty();

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            REQUIRE(dm.getServiceCount() == 4);

            dm.getEventQueue().pushEvent<QuitEvent>(0);
        });

        t.join();
    }

    SECTION("ConstructorInjectionQuitService") {
        std::thread t([]() {
            auto queue = std::make_unique<MultimapQueue>();
            auto &dm = queue->createManager();
            dm.createServiceManager<ConstructorInjectionQuitService>();
            queue->start(CaptureSigInt);
        });

        t.join();
    }

    SECTION("ConstructorInjectionQuitService2") {
        std::thread t([]() {
            auto queue = std::make_unique<MultimapQueue>();
            auto &dm = queue->createManager();
            dm.createServiceManager<ConstructorInjectionQuitService2>();
            queue->start(CaptureSigInt);
        });

        t.join();
    }

    SECTION("ConstructorInjectionQuitService3") {
        std::thread t([]() {
            auto queue = std::make_unique<MultimapQueue>();
            auto &dm = queue->createManager();
            dm.createServiceManager<ConstructorInjectionQuitService3>();
            queue->start(CaptureSigInt);
        });

        t.join();
    }

    SECTION("ConstructorInjectionQuitService4") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        std::thread t([&]() {
            dm.createServiceManager<ConstructorInjectionQuitService4>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            REQUIRE(dm.getServiceCount() == 3);

            dm.getEventQueue().pushEvent<QuitEvent>(0);
        });

        t.join();
    }
}
