#include "Common.h"
#include "TestServices/FailOnStartService.h"
#include "TestServices/UselessService.h"
#include "TestServices/QuitOnStartWithDependenciesService.h"
#include "TestServices/DependencyService.h"
#include "TestServices/MixingInterfacesService.h"
#include "TestServices/TimerRunsOnceService.h"
#include "TestServices/TimerWhileCreatingTimersService.h"
#include "TestServices/TimerStateTransitionsService.h"
#include "TestServices/TimerFactoryEdgeCaseServices.h"
#include "TestServices/CreateTimerService.h"
#include "TestServices/TimerFactoryUserService.h"
#include "TestServices/AddEventHandlerDuringEventHandlingService.h"
#include "TestServices/RequestsLoggingService.h"
#include "TestServices/ConstructorInjectionTestServices.h"
#include "TestServices/RequiredMultipleService.h"
#include "TestServices/DependencyTrackerService.h"
#include "TestServices/AsyncDependencyTrackerService.h"
#include "TestServices/AsyncBroadcastService.h"
#include "TestServices/RemoveAfterAwaitedStopService.h"
#include "TestServices/StopUsingDependencyService.h"
#include "TestServices/AwaitingDependencyService.h"
#include "TestServices/PrivateHandlersService.h"
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/logging/CoutLogger.h>
#include <ichor/services/logging/CoutFrameworkLogger.h>
#include "../examples/common/DebugService.h"


#if defined(TEST_URING)
#include <ichor/event_queues/IOUringQueue.h>
#include <ichor/services/timer/IOUringTimerFactoryFactory.h>
#include <ichor/services/timer/IOUringTimer.h>
#include <ichor/stl/LinuxUtils.h>
#include <catch2/generators/catch_generators.hpp>

#define QIMPL IOUringQueue
#define TFFIMPL IOUringTimerFactoryFactory
#elif defined(TEST_SDEVENT)
#include <ichor/event_queues/SdeventQueue.h>
#include <ichor/services/timer/TimerFactoryFactory.h>

#define QIMPL SdeventQueue
#define TFFIMPL TimerFactoryFactory
#else
#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/services/timer/TimerFactoryFactory.h>
#define TFFIMPL TimerFactoryFactory
#ifdef TEST_ORDERED
#define QIMPL OrderedPriorityQueue
#else
#define QIMPL PriorityQueue
#endif
#endif

bool AddEventHandlerDuringEventHandlingService::_addedReg{};
std::atomic<uint64_t> evtGate;
std::unique_ptr<Ichor::AsyncManualResetEvent> testEvt;
std::atomic<bool> constructorInjectedTargetDestroyed{false};
std::atomic<bool> removeCalled{false};
std::atomic<ServiceIdType> svcIdToPassAround{ServiceIdType{0}};

static void DisplayServices(DependencyManager &dm) {
    auto svcs = dm.getAllServices();
    for(auto &[k, v] : svcs) {
        fmt::println("svc: {}:{} state {}", k, v->getServiceName(), v->getServiceState());
        for(auto &[pk, pv] : v->getProperties()) {
            fmt::println("\tprop: {} {}", pk, pv);
        }
    }
}

#if defined(TEST_URING)
tl::optional<v1::Version> emulateKernelVersion;

TEST_CASE("ServicesTests_uring") {

    auto version = Ichor::v1::kernelVersion();

    REQUIRE(version);
    if(version < v1::Version{.major=5, .minor=18, .patch=0}) {
        return;
    }

    auto gen_i = GENERATE(1, 2);

    if(gen_i == 2) {
        emulateKernelVersion = v1::Version{.major=5, .minor=18, .patch=0};
        fmt::println("emulating kernel version {}", *emulateKernelVersion);
    } else {
        fmt::println("kernel version {}", *version);
    }

#elif defined(TEST_SDEVENT)
TEST_CASE("ServicesTests_sdevent") {
#else
#ifdef TEST_ORDERED
TEST_CASE("ServicesTests_ordered") {
#else
TEST_CASE("ServicesTests") {
#endif
#endif

    testEvt.reset();
    evtGate.store(0, std::memory_order_release);

    SECTION("QuitOnQuitEvent") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        Ichor::v1::Detail::_local_dm = nullptr;

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService, IUselessService>();
            dm.createServiceManager<QuitOnStartWithDependenciesService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("FailOnStartService") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        Ichor::v1::Detail::_local_dm = nullptr;

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<FailOnStartService, IFailOnStartService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto services = dm.getStartedServices<IFailOnStartService>();

            REQUIRE(services.empty());

            auto svcs = dm.getAllServicesOfType<IFailOnStartService>();

            REQUIRE(svcs.size() == 1);
            REQUIRE(svcs[0].first.getStartCount() == 1);

            dm.getEventQueue().pushEvent<QuitEvent>(ServiceIdType{0});
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("FailOnStartWithDependenciesService") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        Ichor::v1::Detail::_local_dm = nullptr;

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService, IUselessService>();
            dm.createServiceManager<FailOnStartWithDependenciesService, IFailOnStartService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto services = dm.getStartedServices<IFailOnStartService>();

            REQUIRE(services.empty());

            auto svcs = dm.getAllServicesOfType<IFailOnStartService>();

            REQUIRE(svcs.size() == 1);
            REQUIRE(svcs[0].first.getStartCount() == 1);

            dm.getEventQueue().pushEvent<QuitEvent>(ServiceIdType{0});
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Required dependencies") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        ServiceIdType secondUselessServiceId{};
        Ichor::v1::Detail::_local_dm = nullptr;

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService, IUselessService>();
            secondUselessServiceId = dm.createServiceManager<UselessService, IUselessService>()->getServiceId();
            dm.createServiceManager<DependencyService<IUselessService, DependencyFlags::REQUIRED | DependencyFlags::ALLOW_MULTIPLE>, ICountService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 2);

            dm.getEventQueue().pushEvent<StopServiceEvent>(ServiceIdType{0}, secondUselessServiceId);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 1);

            dm.getEventQueue().pushEvent<QuitEvent>(ServiceIdType{0});
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Multiple required dependencies, service starts on first and stops when everything uninjected") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        ServiceIdType firstUselessServiceId{};
        ServiceIdType secondUselessServiceId{};
        Ichor::v1::Detail::_local_dm = nullptr;

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            firstUselessServiceId = dm.createServiceManager<UselessService, IUselessService>()->getServiceId();
            dm.createServiceManager<RequiredMultipleService, ICountService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 1);

            secondUselessServiceId = dm.createServiceManager<UselessService, IUselessService>()->getServiceId();
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 2);

            dm.getEventQueue().pushEvent<StopServiceEvent>(ServiceIdType{0}, firstUselessServiceId);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 1);

            dm.getEventQueue().pushEvent<StopServiceEvent>(ServiceIdType{0}, secondUselessServiceId);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 0);

            dm.getEventQueue().pushEvent<QuitEvent>(ServiceIdType{0});
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Multiple required dependencies, service starts on all and stops when everything uninjected") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        ServiceIdType firstUselessServiceId{};
        ServiceIdType secondUselessServiceId{};
        Ichor::v1::Detail::_local_dm = nullptr;

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            firstUselessServiceId = dm.createServiceManager<UselessService, IUselessService>()->getServiceId();
            dm.createServiceManager<RequiredMultipleService2, ICountService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 0);

            secondUselessServiceId = dm.createServiceManager<UselessService, IUselessService>()->getServiceId();
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 2);

            dm.getEventQueue().pushEvent<StopServiceEvent>(ServiceIdType{0}, firstUselessServiceId);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 0);

            dm.getEventQueue().pushEvent<StopServiceEvent>(ServiceIdType{0}, secondUselessServiceId);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 0);

            dm.getEventQueue().pushEvent<QuitEvent>(ServiceIdType{0});
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Optional dependencies") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        ServiceIdType secondUselessServiceId{};
        Ichor::v1::Detail::_local_dm = nullptr;

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService, IUselessService>();
            secondUselessServiceId = dm.createServiceManager<UselessService, IUselessService>()->getServiceId();
            dm.createServiceManager<DependencyService<IUselessService, DependencyFlags::ALLOW_MULTIPLE>, ICountService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);

            REQUIRE(services[0]->getSvcCount() == 2);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<StopServiceEvent>(ServiceIdType{0}, secondUselessServiceId);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);

            REQUIRE(services[0]->getSvcCount() == 1);

            dm.getEventQueue().pushEvent<QuitEvent>(ServiceIdType{0});
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Mixing services should not cause UB") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        Ichor::v1::Detail::_local_dm = nullptr;

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<MixServiceOne, IMixOne, IMixTwo>();
            dm.createServiceManager<MixServiceTwo, IMixOne, IMixTwo>();
            dm.createServiceManager<MixServiceThree, IMixOne, IMixTwo>();
            dm.createServiceManager<MixServiceFour, IMixOne, IMixTwo>();
            dm.createServiceManager<MixServiceFive, IMixOne, IMixTwo>();
            dm.createServiceManager<MixServiceSix, IMixOne, IMixTwo>();
            dm.createServiceManager<CheckMixService, ICountService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        t.join();
    }

    SECTION("TimerService runs exactly once") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        ServiceIdType svcId{};
        Ichor::v1::Detail::_local_dm = nullptr;

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            svcId = dm.createServiceManager<TimerRunsOnceService, ITimerRunsOnceService>()->getServiceId();
            dm.createServiceManager<TFFIMPL>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        auto start = std::chrono::steady_clock::now();
        while(evtGate.load(std::memory_order_acquire) == 0) {
            std::this_thread::sleep_for(500us);
            auto now = std::chrono::steady_clock::now();
            REQUIRE(now - start < 1s);
        }

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto ret = dm.getService<ITimerRunsOnceService>(svcId);
            REQUIRE(ret->first->getCount() == 1);

            dm.getEventQueue().pushEvent<QuitEvent>(svcId);
         });

         t.join();
    }

    SECTION("Running timer keeps working while more timers get created") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        ServiceIdType svcId{};
        Ichor::v1::Detail::_local_dm = nullptr;

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            svcId = dm.createServiceManager<TimerWhileCreatingTimersService, ITimerRunsOnceService>()->getServiceId();
            dm.createServiceManager<TFFIMPL>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        auto start = std::chrono::steady_clock::now();
        while(evtGate.load(std::memory_order_acquire) < 5) {
            std::this_thread::sleep_for(500us);
            auto now = std::chrono::steady_clock::now();
            REQUIRE(now - start < 1s);
        }

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto ret = dm.getService<ITimerRunsOnceService>(svcId);
            REQUIRE(ret->first->getCount() >= 5);

            dm.getEventQueue().pushEvent<QuitEvent>(svcId);
        });

        t.join();
    }

    SECTION("Timers stopping themselves and fire-once timers end up stopped") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        ServiceIdType svcId{};
        Ichor::v1::Detail::_local_dm = nullptr;

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            svcId = dm.createServiceManager<TimerStateTransitionsService, ITimerStateTransitionsService>()->getServiceId();
            dm.createServiceManager<TFFIMPL>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        // both stop callbacks of the self stopping timer, the fire-once timer and the stop callback of the fast timer
        auto start = std::chrono::steady_clock::now();
        while(evtGate.load(std::memory_order_acquire) < 4) {
            std::this_thread::sleep_for(500us);
            auto now = std::chrono::steady_clock::now();
            REQUIRE(now - start < 5s);
        }

        std::atomic<bool> stopped{};
        while(!stopped.load(std::memory_order_acquire)) {
            queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
                auto svc = dm.getService<ITimerStateTransitionsService>(svcId);
                // the thread based timer reaches STOPPED asynchronously
                if(svc->first->getSelfStoppingTimer().getState() == TimerState::STOPPED && svc->first->getFireOnceTimer().getState() == TimerState::STOPPED &&
                   svc->first->getFastTimer().getState() == TimerState::STOPPED) {
                    stopped.store(true, std::memory_order_release);
                }
            });
            std::this_thread::sleep_for(1ms);
            auto now = std::chrono::steady_clock::now();
            REQUIRE(now - start < 5s);
        }

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto svc = dm.getService<ITimerStateTransitionsService>(svcId);
            REQUIRE(svc->first->getSelfStoppedCallbacks() == 2);
            REQUIRE(svc->first->getFireOnceFired() == 1);
            REQUIRE(svc->first->getEarlyStopCallbacks() == 0);

            // a stopped timer calls the stop callback right away, which may use the timer (the thread based Timer used to deadlock on its lock)
            bool called{};
            auto &fireOnce = svc->first->getFireOnceTimer();
            REQUIRE(!fireOnce.stopTimer([&called, &fireOnce]() {
                called = fireOnce.getState() == TimerState::STOPPED;
            }));
            REQUIRE(called);
            // updating the interval of a timer that already fired must not keep the queue busy
            svc->first->getFireOnceTimer().setInterval(1'000'000);

            dm.getEventQueue().pushEvent<QuitEvent>(svcId);
        });

        t.join();
    }

    SECTION("Timer callbacks don't run anymore once the requesting service stopped") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        ServiceIdType svcId{};
        Ichor::v1::Detail::_local_dm = nullptr;

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            svcId = dm.createServiceManager<CountingTimerService>()->getServiceId();
            dm.createServiceManager<TFFIMPL>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        countingTimerCallbacks = 0;
        auto start = std::chrono::steady_clock::now();
        while(countingTimerCallbacks.load(std::memory_order_acquire) < 3) {
            std::this_thread::sleep_for(500us);
            REQUIRE(std::chrono::steady_clock::now() - start < 5s);
        }

        // stopped, not removed: its timer keeps running, but the callbacks shouldn't be called anymore
        queue->pushEvent<StopServiceEvent>(ServiceIdType{0}, svcId);
        std::atomic<bool> stopped{};
        while(!stopped.load(std::memory_order_acquire)) {
            queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
                if(dm.getIService(svcId).value()->getServiceState() == ServiceState::INSTALLED) {
                    stopped.store(true, std::memory_order_release);
                }
            });
            std::this_thread::sleep_for(1ms);
            REQUIRE(std::chrono::steady_clock::now() - start < 5s);
        }
        // callbacks pushed before the service stopped
        std::this_thread::sleep_for(10ms);
        auto const callbacksWhenStopped = countingTimerCallbacks.load(std::memory_order_acquire);
        std::this_thread::sleep_for(50ms);
        REQUIRE(countingTimerCallbacks.load(std::memory_order_acquire) == callbacksWhenStopped);

        queue->pushEvent<QuitEvent>(ServiceIdType{0});

        t.join();
    }

    SECTION("Fire-once timers can restart themselves and stop callbacks can destroy their timer") {
        // a long quit timeout, so that a miscount that makes quitting wait for it stands out from a slow quit (the tests run in parallel)
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(10'000, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(10'000);
#endif
        auto &dm = queue->getManager();
        ServiceIdType svcId{};
        Ichor::v1::Detail::_local_dm = nullptr;

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            svcId = dm.createServiceManager<TimerEdgeCaseService, ITimerEdgeCaseService>()->getServiceId();
            dm.createServiceManager<TFFIMPL>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        auto start = std::chrono::steady_clock::now();
        while(evtGate.load(std::memory_order_acquire) < 1) {
            std::this_thread::sleep_for(500us);
            REQUIRE(std::chrono::steady_clock::now() - start < 5s);
        }

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto svc = dm.getService<ITimerEdgeCaseService>(svcId);
            REQUIRE(svc->first->getRestarts() == 3);
            // stopAllTimers runs while this timer is (likely) still stopping. The timer's own stop callback destroying it must not make
            // the factory miscount its stopped timers: quitting would then hang until the quit timeout.
            svc->first->stopTimerAndDestroyItWhenStopped();
            dm.getEventQueue().pushEvent<QuitEvent>(svcId);
        });

        auto quitStart = std::chrono::steady_clock::now();
        while(queue->is_running()) {
            std::this_thread::sleep_for(1ms);
            // well below the 10 second quit timeout of the queue
            REQUIRE(std::chrono::steady_clock::now() - quitStart < 5s);
        }

        t.join();
    }

#if defined(TEST_URING)
    SECTION("Destroying a running IOUringTimer") {
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
        auto &dm = queue->getManager();
        Ichor::v1::Detail::_local_dm = nullptr;
        std::atomic<uint64_t> fired{};

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
            REQUIRE(queue->createEventLoop());
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto timer = std::make_unique<IOUringTimer>(ScopedServiceProxy<IIOUringQueue*>{queue.get(), ServiceIdType{0}}, 1, ServiceIdType{0});
            timer->setChronoInterval(std::chrono::milliseconds(1));
            timer->setCallback([&fired]() {
                fired++;
            });
            REQUIRE(timer->startTimer());
            // its timeout and handler are still in flight, they must not touch the destroyed timer (detected by ASan)
            timer.reset();
        });

        std::this_thread::sleep_for(20ms);
        // the timeout got removed when the timer was destroyed
        REQUIRE(fired.load(std::memory_order_acquire) <= 1);

        queue->pushEvent<QuitEvent>(ServiceIdType{0});

        t.join();
    }
#endif

    SECTION("TimerRefs own their timer") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        ServiceIdType svcId{};
        Ichor::v1::Detail::_local_dm = nullptr;

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            svcId = dm.createServiceManager<TimerFactoryUserService, ITimerFactoryUserService>()->getServiceId();
            dm.createServiceManager<TFFIMPL>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        auto const start = std::chrono::steady_clock::now();
        std::atomic<bool> svcStarted{};
        while(!svcStarted.load(std::memory_order_acquire)) {
            queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
                if(dm.getService<ITimerFactoryUserService>(svcId)) {
                    svcStarted.store(true, std::memory_order_release);
                }
            });
            std::this_thread::sleep_for(1ms);
            REQUIRE(std::chrono::steady_clock::now() - start < 10s);
        }

        // Runs fn on the thread of the dm, the TimerRefs are only used there
        auto onDm = [&](auto fn) {
            std::atomic<bool> done{};
            queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
                auto svc = dm.getService<ITimerFactoryUserService>(svcId);
                REQUIRE(svc);
                fn(svc->first->getTimerFactory());
                done.store(true, std::memory_order_release);
            });
            while(!done.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(500us);
                REQUIRE(std::chrono::steady_clock::now() - start < 10s);
            }
        };
        auto createdTimers = [&]() {
            size_t amount{};
            onDm([&](ITimerFactory &factory) {
                amount = factory.getCreatedTimers().size();
            });
            return amount;
        };
        auto waitForCreatedTimers = [&](size_t amount) {
            while(createdTimers() != amount) {
                std::this_thread::sleep_for(1ms);
                REQUIRE(std::chrono::steady_clock::now() - start < 10s);
            }
        };
        std::atomic<uint64_t> fired{};
        auto waitForFired = [&](uint64_t amount, std::chrono::milliseconds timeout) {
            auto const waitStart = std::chrono::steady_clock::now();
            while(fired.load(std::memory_order_acquire) < amount) {
                std::this_thread::sleep_for(500us);
                REQUIRE(std::chrono::steady_clock::now() - waitStart < timeout);
            }
        };
        auto createCountingTimer = [&fired](ITimerFactory &factory, std::chrono::milliseconds interval) {
            auto timer = factory.createTimer();
            timer.setChronoInterval(interval);
            timer.setCallback([&fired]() {
                fired.fetch_add(1, std::memory_order_acq_rel);
            });
            return timer;
        };

        tl::optional<TimerRef> timer;
        tl::optional<TimerRef> copy;

        // copies share ownership
        onDm([&](ITimerFactory &factory) {
            timer = createCountingTimer(factory, 1ms);
            REQUIRE(timer->startTimer());
            copy = *timer;
        });
        waitForFired(fired.load(std::memory_order_acquire) + 3, 5'000ms);
        REQUIRE(createdTimers() == 1);

        onDm([&](ITimerFactory &) {
            timer.reset();
        });
        waitForFired(fired.load(std::memory_order_acquire) + 3, 5'000ms);
        REQUIRE(createdTimers() == 1);

        // the last one stops and destroys it
        onDm([&](ITimerFactory &) {
            copy.reset();
        });
        waitForCreatedTimers(0);
        auto const firedWhenDestroyed = fired.load(std::memory_order_acquire);
        std::this_thread::sleep_for(20ms);
        REQUIRE(fired.load(std::memory_order_acquire) == firedWhenDestroyed);

        // also when it was only just started
        onDm([&](ITimerFactory &factory) {
            auto justStarted = createCountingTimer(factory, 1ms);
            REQUIRE(justStarted.startTimer());
        });
        waitForCreatedTimers(0);

        // a stopped one is destroyed right away
        onDm([&](ITimerFactory &factory) {
            static_cast<void>(createCountingTimer(factory, 1ms));
            REQUIRE(factory.getCreatedTimers().empty());
        });

        // a timer destroyed through its factory leaves its TimerRefs without a timer
        onDm([&](ITimerFactory &factory) {
            timer = createCountingTimer(factory, 1ms);
            REQUIRE(factory.destroyTimerIfStopped(timer->getTimerId()));
            REQUIRE(factory.getCreatedTimers().empty());
            timer.reset();
        });

        // a detached timer keeps running until its factory stops
        onDm([&](ITimerFactory &factory) {
            auto detached = createCountingTimer(factory, 1ms);
            REQUIRE(detached.startTimer());
            detached.detach();
        });
        waitForFired(fired.load(std::memory_order_acquire) + 3, 5'000ms);
        REQUIRE(createdTimers() == 1);

        // changing the interval and stopping a running timer take effect right away, not after the old interval
        // the detached timer keeps incrementing fired, so this one counts by itself
        std::atomic<uint64_t> shortenedFired{};
        onDm([&](ITimerFactory &factory) {
            timer = factory.createTimer();
            timer->setChronoInterval(10'000ms);
            timer->setCallback([&shortenedFired]() {
                shortenedFired.fetch_add(1, std::memory_order_acq_rel);
            });
            REQUIRE(timer->startTimer());
        });
        onDm([&](ITimerFactory &) {
            timer->setChronoInterval(1ms);
        });
        auto const shortenedAt = std::chrono::steady_clock::now();
        while(shortenedFired.load(std::memory_order_acquire) == 0) {
            std::this_thread::sleep_for(500us);
            REQUIRE(std::chrono::steady_clock::now() - shortenedAt < 2s);
        }

        std::atomic<bool> stopped{};
        onDm([&](ITimerFactory &) {
            timer->setChronoInterval(10'000ms);
            REQUIRE(timer->stopTimer([&stopped]() {
                stopped.store(true, std::memory_order_release);
            }));
        });
        auto const stoppedAt = std::chrono::steady_clock::now();
        while(!stopped.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(500us);
            REQUIRE(std::chrono::steady_clock::now() - stoppedAt < 500ms);
        }

        onDm([&](ITimerFactory &) {
            timer.reset();
        });
        REQUIRE(createdTimers() == 1);

        queue->pushEvent<QuitEvent>(ServiceIdType{0});

        t.join();
    }

    SECTION("Created timers can be requested") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        ServiceIdType creatingSvcId1{};
        ServiceIdType creatingSvcId2{};
        ServiceIdType creatingSvcId3{};
        ServiceIdType tffSvcId{};
        Ichor::v1::Detail::_local_dm = nullptr;

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            creatingSvcId1 = dm.createServiceManager<CreateTimerService<1>>()->getServiceId();
            creatingSvcId2 = dm.createServiceManager<CreateTimerService<2>>()->getServiceId();
            creatingSvcId3 = dm.createServiceManager<CreateTimerService<3>>()->getServiceId();
            tffSvcId = dm.createServiceManager<TFFIMPL, ITimerTimerFactory>()->getServiceId();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        auto start = std::chrono::steady_clock::now();
        while(evtGate.load(std::memory_order_acquire) < 6) {
            std::this_thread::sleep_for(500us);
            auto now = std::chrono::steady_clock::now();
            REQUIRE(now - start < 1s);
        }

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto ret = dm.getService<ITimerTimerFactory>(tffSvcId);
            REQUIRE(ret);
            auto factoryIds = ret->first->getCreatedTimerFactoryIds();
            REQUIRE(factoryIds.size() == 3);
            auto getCorrectFactory = [](auto &dm2, auto &ids, ServiceIdType id) -> tl::optional<std::pair<ITimerFactory*, IService*>> {
                auto factory = dm2.template getService<ITimerFactory>(ids[0]);
                if(factory->first->getRequestingServiceId() == id) {
                    return factory;
                }
                factory = dm2.template getService<ITimerFactory>(ids[1]);
                if(factory->first->getRequestingServiceId() == id) {
                    return factory;
                }
                factory = dm2.template getService<ITimerFactory>(ids[2]);
                if(factory->first->getRequestingServiceId() == id) {
                    return factory;
                }
                REQUIRE(false);
                return {};
            };

            auto factory1 = getCorrectFactory(dm, factoryIds, creatingSvcId1);
            auto factory2 = getCorrectFactory(dm, factoryIds, creatingSvcId2);
            auto factory3 = getCorrectFactory(dm, factoryIds, creatingSvcId3);

            auto timers1 = factory1->first->getCreatedTimers();
            auto timers2 = factory2->first->getCreatedTimers();
            auto timers3 = factory3->first->getCreatedTimers();

            auto reqTimer = factory1->first->getTimerById(timers1[0]->getTimerId());
            REQUIRE(reqTimer);
            REQUIRE((*reqTimer)->getTimerId() == timers1[0]->getTimerId());

            REQUIRE(timers1.size() == 1);
            REQUIRE(timers2.size() == 2);
            REQUIRE(timers3.size() == 3);

            dm.getEventQueue().pushEvent<QuitEvent>(tffSvcId);
        });

        t.join();
    }

    SECTION("Add event handler during event handling") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        Ichor::v1::Detail::_local_dm = nullptr;

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<AddEventHandlerDuringEventHandlingService>();
            dm.createServiceManager<AddEventHandlerDuringEventHandlingService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<TestEvent>(ServiceIdType{0});

        runForOrQueueEmpty(dm);

        queue->pushEvent<QuitEvent>(ServiceIdType{0});

        t.join();
    }

    SECTION("Services can keep the functions the framework calls private") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        Ichor::v1::Detail::_local_dm = nullptr;

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<PrivateHandlersService, IPrivateHandlersService>();
            dm.createServiceManager<UselessService, IUselessService>();
            dm.createServiceManager<DependencyService<IUselessService, DependencyFlags::ALLOW_MULTIPLE>, ICountService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<TestEvent>(ServiceIdType{0});

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto services = dm.getStartedServices<IPrivateHandlersService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->getDependencyCount() == 1);
            REQUIRE(services[0]->getTrackerRequestCount() >= 1);
            REQUIRE(services[0]->getHandledEventCount() == 1);
            REQUIRE(services[0]->getPreInterceptedCount() == 1);
            REQUIRE(services[0]->getPostInterceptedCount() == 1);

            queue->pushEvent<QuitEvent>(ServiceIdType{0});
        });

        t.join();
    }

    SECTION("LoggerAdmin removes logger when service is gone") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        ServiceIdType svcId{};
        Ichor::v1::Detail::_local_dm = nullptr;

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>();
            svcId = dm.createServiceManager<RequestsLoggingService, IRequestsLoggingService>()->getServiceId();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            DisplayServices(dm);
#if defined(TEST_URING) || defined(TEST_SDEVENT)
            REQUIRE(dm.getServiceCount() == 7);
#else
            REQUIRE(dm.getServiceCount() == 6);
#endif

            dm.getEventQueue().pushEvent<StopServiceEvent>(ServiceIdType{0}, svcId, true);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            DisplayServices(dm);
#if defined(TEST_URING) || defined(TEST_SDEVENT)
            REQUIRE(dm.getServiceCount() == 5);
#else
            REQUIRE(dm.getServiceCount() == 4);
#endif

            dm.getEventQueue().pushEvent<QuitEvent>(ServiceIdType{0});
        });

        t.join();
    }

    SECTION("ConstructorInjectionService basic test") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        ServiceIdType svcId{};
        Ichor::v1::Detail::_local_dm = nullptr;

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>();
            dm.createServiceManager<DependencyService<IUselessService, DependencyFlags::ALLOW_MULTIPLE>, ICountService>();
            auto service = dm.createServiceManager<ConstructorInjectionTestService, IConstructorInjectionTestService>();
            svcId = service->getServiceId();
            static_assert(std::is_same_v<decltype(service), ServiceProtectedPointer<IService>>);
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            DisplayServices(dm);

            DisplayServices(dm);
#if defined(TEST_URING) || defined(TEST_SDEVENT)
            REQUIRE(dm.getServiceCount() == 8);
#else
            REQUIRE(dm.getServiceCount() == 7);
#endif
            auto svcs = dm.getAllServicesOfType<IConstructorInjectionTestService>();
            REQUIRE(svcs.size() == 1);
            REQUIRE(svcs[0].second.getServiceId() == svcId);

            dm.getEventQueue().pushEvent<StopServiceEvent>(ServiceIdType{0}, svcId, true);
        });

        runForOrQueueEmpty(dm);

        // the qemu setup used in build.sh is not fast enough to have the test pass.
#ifdef ICHOR_AARCH64
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
#endif

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            DisplayServices(dm);
            DisplayServices(dm);
#if defined(TEST_URING) || defined(TEST_SDEVENT)
            REQUIRE(dm.getServiceCount() == 6);
#else
            REQUIRE(dm.getServiceCount() == 5);
#endif

            dm.getEventQueue().pushEvent<QuitEvent>(ServiceIdType{0});
        });

        t.join();
    }

    SECTION("ConstructorInjectionQuitService") {
        ServiceIdType dbgSvcId{};
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        Ichor::v1::Detail::_local_dm = nullptr;

        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<ConstructorInjectionQuitService>();
            dbgSvcId = dm.createServiceManager<DebugService, IDebugService>()->getServiceId();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        auto start = std::chrono::steady_clock::now();
        while(queue->is_running()) {
            std::this_thread::sleep_for(10ms);
            auto now = std::chrono::steady_clock::now();
            if(now - start >= 1s) {
                queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
                    auto dbgSvc = dm.getService<IDebugService>(dbgSvcId);
                    REQUIRE(dbgSvc);
                    dbgSvc->first->printServices();
                });
                std::this_thread::sleep_for(10ms);
                break;
            }
        }

        auto now = std::chrono::steady_clock::now();
        REQUIRE(now - start < 1s);

        t.join();
    }

    SECTION("ConstructorInjectionQuitService2") {
        std::thread t([]() {
#if defined(TEST_URING)
            auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
            auto queue = std::make_unique<QIMPL>(500);
#endif
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            auto &dm = queue->getManager();
            dm.createServiceManager<ConstructorInjectionQuitService2>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        t.join();
    }

    SECTION("ConstructorInjectionQuitService3") {
        std::thread t([]() {
#if defined(TEST_URING)
            auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
            auto queue = std::make_unique<QIMPL>(500);
#endif
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            auto &dm = queue->getManager();
            dm.createServiceManager<ConstructorInjectionQuitService3>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        t.join();
    }

    SECTION("ConstructorInjectionQuitService4") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        Ichor::v1::Detail::_local_dm = nullptr;
        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<ConstructorInjectionQuitService4>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            DisplayServices(dm);
#if defined(TEST_URING) || defined(TEST_SDEVENT)
            REQUIRE(dm.getServiceCount() == 4);
#else
            REQUIRE(dm.getServiceCount() == 3);
#endif

            dm.getEventQueue().pushEvent<QuitEvent>(ServiceIdType{0});
        });

        t.join();
    }

    SECTION("Multiple dependency trackers") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        ServiceIdType trackerSvcId{};
        ServiceIdType depSvcId{};
        Ichor::v1::Detail::_local_dm = nullptr;
        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<DependencyTrackerService<IUselessService, UselessService, IUselessService>>();
            trackerSvcId = dm.createServiceManager<DependencyTrackerService<IUselessService, UselessService2, IUselessService>>()->getServiceId();
            depSvcId = dm.createServiceManager<DependencyService<IUselessService, DependencyFlags::ALLOW_MULTIPLE>, ICountService>()->getServiceId();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            DisplayServices(dm);
#if defined(TEST_URING) || defined(TEST_SDEVENT)
            REQUIRE(dm.getServiceCount() == 8);
#else
            REQUIRE(dm.getServiceCount() == 7);
#endif
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 2);


            queue->pushEvent<StopServiceEvent>(ServiceIdType{0}, depSvcId, true);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            DisplayServices(dm);
#if defined(TEST_URING) || defined(TEST_SDEVENT)
            REQUIRE(dm.getServiceCount() == 5);
#else
            REQUIRE(dm.getServiceCount() == 4);
#endif
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 0);

            queue->pushEvent<StopServiceEvent>(ServiceIdType{0}, trackerSvcId, true);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            DisplayServices(dm);
            DisplayServices(dm);
#if defined(TEST_URING) || defined(TEST_SDEVENT)
            REQUIRE(dm.getServiceCount() == 4);
#else
            REQUIRE(dm.getServiceCount() == 3);
#endif
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 0);

            depSvcId = dm.createServiceManager<DependencyService<IUselessService, DependencyFlags::ALLOW_MULTIPLE>, ICountService>()->getServiceId();
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            DisplayServices(dm);
            DisplayServices(dm);
#if defined(TEST_URING) || defined(TEST_SDEVENT)
            REQUIRE(dm.getServiceCount() == 6);
#else
            REQUIRE(dm.getServiceCount() == 5);
#endif
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 1);

            queue->pushEvent<QuitEvent>(ServiceIdType{0});
        });

        t.join();
    }

    SECTION("Async multiple dependency trackers") {
        auto queue = std::make_unique<QIMPL>(500);
        auto &dm = queue->getManager();
        ServiceIdType trackerSvcId{};
        ServiceIdType depSvcId{};
        testEvt = std::make_unique<AsyncManualResetEvent>();
        Ichor::v1::Detail::_local_dm = nullptr;
        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<AsyncDependencyTrackerService<IUselessService, UselessService, IUselessService>>();
            trackerSvcId = dm.createServiceManager<AsyncDependencyTrackerService<IUselessService, UselessService2, IUselessService>>()->getServiceId();
            depSvcId = dm.createServiceManager<DependencyService<IUselessService, DependencyFlags::ALLOW_MULTIPLE>, ICountService>()->getServiceId();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            DisplayServices(dm);
#if defined(TEST_URING) || defined(TEST_SDEVENT)
            REQUIRE(dm.getServiceCount() == 6);
#else
            REQUIRE(dm.getServiceCount() == 5);
#endif
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 0);

            testEvt->set();
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            DisplayServices(dm);
#if defined(TEST_URING) || defined(TEST_SDEVENT)
            REQUIRE(dm.getServiceCount() == 8);
#else
            REQUIRE(dm.getServiceCount() == 7);
#endif
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 2);

            testEvt->reset();

            queue->pushEvent<StopServiceEvent>(ServiceIdType{0}, depSvcId, true);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            DisplayServices(dm);
#if defined(TEST_URING) || defined(TEST_SDEVENT)
            REQUIRE(dm.getServiceCount() == 7);
#else
            REQUIRE(dm.getServiceCount() == 6);
#endif
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 0);

            testEvt->set();
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            DisplayServices(dm);
#if defined(TEST_URING) || defined(TEST_SDEVENT)
            REQUIRE(dm.getServiceCount() == 5);
#else
            REQUIRE(dm.getServiceCount() == 4);
#endif
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 0);

            queue->pushEvent<StopServiceEvent>(ServiceIdType{0}, trackerSvcId, true);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            DisplayServices(dm);
            DisplayServices(dm);
#if defined(TEST_URING) || defined(TEST_SDEVENT)
            REQUIRE(dm.getServiceCount() == 4);
#else
            REQUIRE(dm.getServiceCount() == 3);
#endif
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 0);

            depSvcId = dm.createServiceManager<DependencyService<IUselessService, DependencyFlags::ALLOW_MULTIPLE>, ICountService>()->getServiceId();
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            DisplayServices(dm);
            DisplayServices(dm);
#if defined(TEST_URING) || defined(TEST_SDEVENT)
            REQUIRE(dm.getServiceCount() == 6);
#else
            REQUIRE(dm.getServiceCount() == 5);
#endif
            auto services = dm.getStartedServices<ICountService>();

            REQUIRE(services.size() == 1);
            REQUIRE(services[0]->isRunning());
            REQUIRE(services[0]->getSvcCount() == 1);

            queue->pushEvent<QuitEvent>(ServiceIdType{0});
        });

        t.join();
    }

    SECTION("Async Multiple Broadcast Handlers") {
        auto queue = std::make_unique<QIMPL>(500);
        auto &dm = queue->getManager();
        testEvt = std::make_unique<AsyncManualResetEvent>();
        Ichor::v1::Detail::_local_dm = nullptr;
        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<AsyncBroadcastService, IAsyncBroadcastService>();
            dm.createServiceManager<AsyncBroadcastService, IAsyncBroadcastService>();
            dm.createServiceManager<AsyncBroadcastService, IAsyncBroadcastService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, [&]() -> AsyncGenerator<IchorBehaviour> {
            DisplayServices(dm);
            auto services = dm.getStartedServices<IAsyncBroadcastService>();

            REQUIRE(services.size() == 3);
            for (auto &svc: services) {
                REQUIRE(svc->getFinishedCalls() == 0);
                REQUIRE(svc->getFinishedPosts() == 0);
            }

            for (auto &svc: services) {
                co_await svc->postEvent();
            }

            co_return {};
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            DisplayServices(dm);
            auto services = dm.getStartedServices<IAsyncBroadcastService>();

            REQUIRE(services.size() == 3);
            for (auto &svc: services) {
                REQUIRE(svc->getFinishedCalls() == 0);
                REQUIRE(svc->getFinishedPosts() == 0);
            }

            testEvt->set();
            for (auto &svc: services) {
                REQUIRE(svc->getFinishedCalls() == 1);
                REQUIRE(svc->getFinishedPosts() == 0);
            }
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            DisplayServices(dm);
            auto services = dm.getStartedServices<IAsyncBroadcastService>();

            for (auto &svc: services) {
                REQUIRE(svc->getFinishedCalls() == 3);
                REQUIRE(svc->getFinishedPosts() == 1);
            }

            dm.getEventQueue().pushEvent<QuitEvent>(ServiceIdType{0});
        });

        t.join();
    }

    SECTION("RemoveAfterAwaitedStopService") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        Ichor::v1::Detail::_local_dm = nullptr;
        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<RemoveAfterAwaitedStopService<true>, IRemoveAfterAwaitedStopService>();
            dm.createServiceManager<UselessService, IUselessService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        t.join();
    }

    SECTION("RemoveAfterAwaitedStopService with dependee") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        Ichor::v1::Detail::_local_dm = nullptr;
        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<RemoveAfterAwaitedStopService<false>, IRemoveAfterAwaitedStopService>();
            dm.createServiceManager<UselessService, IUselessService>();
            dm.createServiceManager<DependingOnRemoveAfterAwaitedStopService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        t.join();
    }

    SECTION("Dependency going offline while a dependee's stop is suspended stays until that stop is done") {
        StopUsingDependencyService::dependencyPresentAfterSuspending = false;
        StopUsingDependencyService::removedWhileStopping = false;
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        Ichor::v1::Detail::_local_dm = nullptr;
        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<StopUsingDependencyService>();
            dm.createServiceManager<UselessService, IUselessService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        t.join();

        REQUIRE(StopUsingDependencyService::dependencyPresentAfterSuspending);
        REQUIRE(!StopUsingDependencyService::removedWhileStopping);
    }

    SECTION("Dependency retains dependees while async dependencyOffline runs") {
        using namespace std::chrono_literals;

        ServiceIdType providerId{};
        testEvt = std::make_unique<AsyncManualResetEvent>();

#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->getManager();
        Ichor::v1::Detail::_local_dm = nullptr;
        std::thread t([&]() {
            Ichor::v1::Detail::_local_dm = &dm;
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            providerId = dm.createServiceManager<UselessService, IUselessService>()->getServiceId();
            dm.createServiceManager<AwaitingDependencyService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            dm.getEventQueue().pushEvent<StopServiceEvent>(ServiceIdType{0}, providerId, true);
        });

        auto start = std::chrono::steady_clock::now();
        while(evtGate.load(std::memory_order_acquire) == 0) {
            std::this_thread::sleep_for(100us);
            REQUIRE(std::chrono::steady_clock::now() - start < 5s);
        }

        queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
            auto deps = dm.getDependentsForService(providerId);
            REQUIRE_FALSE(deps.empty());
            testEvt->set();

            queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&dm, providerId]() {
                auto deps2 = dm.getDependentsForService(providerId);
                REQUIRE(deps2.empty());
            });
        });

        queue->pushEvent<QuitEvent>(ServiceIdType{0});

        t.join();
    }
}
