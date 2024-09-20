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
#include "TestServices/RequiredMultipleService.h"
#include "TestServices/DependencyTrackerService.h"
#include "TestServices/RemoveAfterAwaitedStopService.h"
#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/logging/CoutLogger.h>
#include <ichor/services/timer/TimerFactoryFactory.h>

bool AddEventHandlerDuringEventHandlingService::_addedReg{};

static void DisplayServices(DependencyManager &dm) {
    auto svcs = dm.getAllServices();
    for(auto &[k, v] : svcs) {
        fmt::println("svc: {}:{} state {}", k, v->getServiceName(), v->getServiceState());
        for(auto &[pk, pv] : v->getProperties()) {
            fmt::println("\tprop: {} {}", pk, pv);
        }
    }
}

TEST_CASE("ServicesTests") {

    SECTION("QuitOnQuitEvent") {
        auto queue = std::make_unique<PriorityQueue>();
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
        auto queue = std::make_unique<OrderedPriorityQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<FailOnStartService, IFailOnStartService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

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
        auto queue = std::make_unique<OrderedPriorityQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService, IUselessService>();
            dm.createServiceManager<FailOnStartWithDependenciesService, IFailOnStartService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

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
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        uint64_t secondUselessServiceId{};

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService, IUselessService>();
            secondUselessServiceId = dm.createServiceManager<UselessService, IUselessService>()->getServiceId();
            dm.createServiceManager<DependencyService<IUselessService, DependencyFlags::REQUIRED | DependencyFlags::ALLOW_MULTIPLE>, ICountService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 2);

            dm.getEventQueue().pushEvent<StopServiceEvent>(0, secondUselessServiceId);
        });

        runForOrQueueEmpty(dm);

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

    SECTION("Multiple required dependencies, service starts on first and stops when everything uninjected") {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        uint64_t firstUselessServiceId{};
        uint64_t secondUselessServiceId{};

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            firstUselessServiceId = dm.createServiceManager<UselessService, IUselessService>()->getServiceId();
            dm.createServiceManager<RequiredMultipleService, ICountService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 1);

            secondUselessServiceId = dm.createServiceManager<UselessService, IUselessService>()->getServiceId();
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 2);

            dm.getEventQueue().pushEvent<StopServiceEvent>(0, firstUselessServiceId);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 1);

            dm.getEventQueue().pushEvent<StopServiceEvent>(0, secondUselessServiceId);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 0);

            dm.getEventQueue().pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Multiple required dependencies, service starts on all and stops when everything uninjected") {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        uint64_t firstUselessServiceId{};
        uint64_t secondUselessServiceId{};

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            firstUselessServiceId = dm.createServiceManager<UselessService, IUselessService>()->getServiceId();
            dm.createServiceManager<RequiredMultipleService2, ICountService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 0);

            secondUselessServiceId = dm.createServiceManager<UselessService, IUselessService>()->getServiceId();
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 2);

            dm.getEventQueue().pushEvent<StopServiceEvent>(0, firstUselessServiceId);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 0);

            dm.getEventQueue().pushEvent<StopServiceEvent>(0, secondUselessServiceId);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 0);

            dm.getEventQueue().pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Optional dependencies") {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        uint64_t secondUselessServiceId{};

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService, IUselessService>();
            secondUselessServiceId = dm.createServiceManager<UselessService, IUselessService>()->getServiceId();
            dm.createServiceManager<DependencyService<IUselessService, DependencyFlags::ALLOW_MULTIPLE>, ICountService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);

            REQUIRE(services[0]->getSvcCount() == 2);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<StopServiceEvent>(0, secondUselessServiceId);

        runForOrQueueEmpty(dm);

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
        auto queue = std::make_unique<PriorityQueue>();
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
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        uint64_t svcId{};

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            svcId = dm.createServiceManager<TimerRunsOnceService, ITimerRunsOnceService>()->getServiceId();
            dm.createServiceManager<TimerFactoryFactory>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            auto ret = dm.getService<ITimerRunsOnceService>(svcId);
            REQUIRE(ret->first->getCount() == 1);

            dm.getEventQueue().pushEvent<QuitEvent>(svcId);
        });

        t.join();
    }

    SECTION("Add event handler during event handling") {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<AddEventHandlerDuringEventHandlingService>();
            dm.createServiceManager<AddEventHandlerDuringEventHandlingService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<TestEvent>(0);

        runForOrQueueEmpty(dm);

        queue->pushEvent<QuitEvent>(0);

        t.join();
    }

    SECTION("LoggerAdmin removes logger when service is gone") {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        uint64_t svcId{};

        std::thread t([&]() {
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>();
            svcId = dm.createServiceManager<RequestsLoggingService, IRequestsLoggingService>()->getServiceId();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            REQUIRE(dm.getServiceCount() == 5);

            dm.getEventQueue().pushEvent<StopServiceEvent>(0, svcId, true);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            REQUIRE(dm.getServiceCount() == 3);

            dm.getEventQueue().pushEvent<QuitEvent>(0);
        });

        t.join();
    }

    SECTION("ConstructorInjectionService basic test") {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        uint64_t svcId{};

        std::thread t([&]() {
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>();
            dm.createServiceManager<DependencyService<IUselessService, DependencyFlags::ALLOW_MULTIPLE>, ICountService>();
            auto service = dm.createServiceManager<ConstructorInjectionTestService, IConstructorInjectionTestService>();
            svcId = service->getServiceId();
            static_assert(std::is_same_v<decltype(service), ServiceProtectedPointer<IService>>, "");
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            DisplayServices(dm);
            REQUIRE(dm.getServiceCount() == 6);
            auto svcs = dm.getAllServicesOfType<IConstructorInjectionTestService>();
            REQUIRE(svcs.size() == 1);
            REQUIRE(svcs[0].second.getServiceId() == svcId);

            dm.getEventQueue().pushEvent<StopServiceEvent>(0, svcId, true);
        });

        runForOrQueueEmpty(dm);

        // the qemu setup used in build.sh is not fast enough to have the test pass.
#ifdef ICHOR_AARCH64
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
#endif

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            DisplayServices(dm);
            REQUIRE(dm.getServiceCount() == 4);

            dm.getEventQueue().pushEvent<QuitEvent>(0);
        });

        t.join();
    }

    SECTION("ConstructorInjectionQuitService") {
        std::thread t([]() {
            auto queue = std::make_unique<PriorityQueue>();
            auto &dm = queue->createManager();
            dm.createServiceManager<ConstructorInjectionQuitService>();
            queue->start(CaptureSigInt);
        });

        t.join();
    }

    SECTION("ConstructorInjectionQuitService2") {
        std::thread t([]() {
            auto queue = std::make_unique<PriorityQueue>();
            auto &dm = queue->createManager();
            dm.createServiceManager<ConstructorInjectionQuitService2>();
            queue->start(CaptureSigInt);
        });

        t.join();
    }

    SECTION("ConstructorInjectionQuitService3") {
        std::thread t([]() {
            auto queue = std::make_unique<PriorityQueue>();
            auto &dm = queue->createManager();
            dm.createServiceManager<ConstructorInjectionQuitService3>();
            queue->start(CaptureSigInt);
        });

        t.join();
    }

    SECTION("ConstructorInjectionQuitService4") {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        std::thread t([&]() {
            dm.createServiceManager<ConstructorInjectionQuitService4>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            REQUIRE(dm.getServiceCount() == 3);

            dm.getEventQueue().pushEvent<QuitEvent>(0);
        });

        t.join();
    }

    SECTION("Multiple dependency trackers") {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        ServiceIdType trackerSvcId{};
        ServiceIdType depSvcId{};
        std::thread t([&]() {
            dm.createServiceManager<DependencyTrackerService<IUselessService, UselessService, IUselessService>>();
            trackerSvcId = dm.createServiceManager<DependencyTrackerService<IUselessService, UselessService2, IUselessService>>()->getServiceId();
            depSvcId = dm.createServiceManager<DependencyService<IUselessService, DependencyFlags::ALLOW_MULTIPLE>, ICountService>()->getServiceId();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            DisplayServices(dm);
            REQUIRE(dm.getServiceCount() == 7);
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 2);


            queue->pushEvent<StopServiceEvent>(0, depSvcId, true);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            DisplayServices(dm);
            REQUIRE(dm.getServiceCount() == 4);
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 0);

            queue->pushEvent<StopServiceEvent>(0, trackerSvcId, true);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            DisplayServices(dm);
            REQUIRE(dm.getServiceCount() == 3);
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 0);

            depSvcId = dm.createServiceManager<DependencyService<IUselessService, DependencyFlags::ALLOW_MULTIPLE>, ICountService>()->getServiceId();
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            DisplayServices(dm);
            REQUIRE(dm.getServiceCount() == 5);
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 1);

            queue->pushEvent<QuitEvent>(0);
        });

        t.join();
    }

    SECTION("RemoveAfterAwaitedStopService") {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        std::thread t([&]() {
            dm.createServiceManager<RemoveAfterAwaitedStopService<true>, IRemoveAfterAwaitedStopService>();
            dm.createServiceManager<UselessService, IUselessService>();
            queue->start(CaptureSigInt);
        });

        t.join();
    }

    SECTION("RemoveAfterAwaitedStopService with dependee") {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        std::thread t([&]() {
            dm.createServiceManager<RemoveAfterAwaitedStopService<false>, IRemoveAfterAwaitedStopService>();
            dm.createServiceManager<UselessService, IUselessService>();
            dm.createServiceManager<DependingOnRemoveAfterAwaitedStopService>();
            queue->start(CaptureSigInt);
        });

        t.join();
    }
}
