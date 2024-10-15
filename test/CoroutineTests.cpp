#include "Common.h"
#include "TestServices/GeneratorService.h"
#include "TestServices/AwaitService.h"
#include "TestServices/MultipleAwaitersService.h"
#include "TestServices/AsyncUsingTimerService.h"
#include "TestServices/AwaitReturnService.h"
#include "TestServices/StopsInAsyncStartService.h"
#include "TestServices/DependencyOfflineWhileStartingService.h"
#include "TestServices/DependencyOnlineWhileStoppingService.h"
#include <ichor/events/RunFunctionEvent.h>
#include <memory>

std::unique_ptr<Ichor::AsyncManualResetEvent> _evt;
std::unique_ptr<Ichor::AsyncAutoResetEvent> _autoEvt;
uint64_t AwaitNoCopy::countConstructed{};
uint64_t AwaitNoCopy::countDestructed{};
uint64_t AwaitNoCopy::countMoved{};

#if defined(TEST_URING)
#include <ichor/event_queues/IOUringQueue.h>
#include <ichor/services/timer/IOUringTimerFactoryFactory.h>
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

#if defined(TEST_URING)
tl::optional<Version> emulateKernelVersion;

TEST_CASE("CoroutineTests_uring") {

    auto version = Ichor::kernelVersion();

    REQUIRE(version);
    if(version < Version{5, 18, 0}) {
        return;
    }

    auto gen_i = GENERATE(1, 2);

    if(gen_i == 2) {
        emulateKernelVersion = Version{5, 18, 0};
        fmt::println("emulating kernel version {}", *emulateKernelVersion);
    } else {
        fmt::println("kernel version {}", *version);
    }

#elif defined(TEST_SDEVENT)
TEST_CASE("CoroutineTests_sdevent") {
#else
#ifdef TEST_ORDERED
TEST_CASE("CoroutineTests_ordered") {
#else
TEST_CASE("CoroutineTests") {
#endif
#endif

    _evt = std::make_unique<Ichor::AsyncManualResetEvent>();

    SECTION("Required dependencies") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->createManager();

        std::thread t([&]() {
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<GeneratorService, IGeneratorService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour>{
            auto services = dm.getStartedServices<IGeneratorService>();

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

            dm.getEventQueue().pushEvent<QuitEvent>(0);
            INTERNAL_DEBUG("quit");

            co_return {};
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("co_await in function") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->createManager();

        std::thread t([&]() {
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<AwaitService, IAwaitService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            auto services = dm.getStartedServices<IAwaitService>();

            REQUIRE(services.size() == 1);

            INTERNAL_DEBUG("before");

            co_await services[0]->await_something();

            INTERNAL_DEBUG("after");

            co_yield {};

            INTERNAL_DEBUG("quit");

            dm.getEventQueue().pushEvent<QuitEvent>(0);

            INTERNAL_DEBUG("after2");

            co_return {};
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, []() {
            INTERNAL_DEBUG("set");
            _evt->set();
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("co_await in event handler") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->createManager();

        std::thread t([&]() {
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<EventAwaitService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<AwaitEvent>(0);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, []() {
            INTERNAL_DEBUG("set");
            _evt->set();
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("multiple awaiters in event handler") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->createManager();
        _autoEvt = std::make_unique<Ichor::AsyncAutoResetEvent>();
        ServiceIdType svcId{};

        std::thread t([&]() {
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            svcId = dm.createServiceManager<MultipleAwaitService, IMultipleAwaitService>()->getServiceId();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            INTERNAL_DEBUG("set");
            auto svc = dm.getService<IMultipleAwaitService>(svcId);
            REQUIRE(svc);
            _autoEvt->set_all();
            REQUIRE((*svc).first->getCount() == 2);
            dm.getEventQueue().pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("co_await in timer service") {
        fmt::print("co_await in timer service\n");
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->createManager();

        std::thread t([&]() {
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<AwaitService, IAwaitService>();
            dm.createServiceManager<AsyncUsingTimerService>();
            dm.createServiceManager<TFFIMPL>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, []() {
            INTERNAL_DEBUG("set");
            _evt->set();
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("co_await user defined struct generator") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->createManager();
        ServiceIdType svcId{};
        AwaitNoCopy::countConstructed = 0;
        AwaitNoCopy::countDestructed = 0;
        AwaitNoCopy::countMoved = 0;

        std::thread t([&]() {
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            svcId = dm.createServiceManager<AwaitReturnService, IAwaitReturnService>()->getServiceId();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            auto svc = dm.getService<IAwaitReturnService>(svcId);
            REQUIRE(svc);
            auto &await = *co_await (*svc).first->Await().begin();
            co_return {};
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&dm = dm]() {
            INTERNAL_DEBUG("set");
            _evt->set();
            dm.getEventQueue().pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE(AwaitNoCopy::countConstructed == 1);
        REQUIRE(AwaitNoCopy::countDestructed == 2);
        REQUIRE(AwaitNoCopy::countMoved == 1);

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("co_await user defined struct task") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->createManager();
        ServiceIdType svcId{};
        AwaitNoCopy::countConstructed = 0;
        AwaitNoCopy::countDestructed = 0;
        AwaitNoCopy::countMoved = 0;

        std::thread t([&]() {
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            svcId = dm.createServiceManager<AwaitReturnService, IAwaitReturnService>()->getServiceId();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            auto svc = dm.getService<IAwaitReturnService>(svcId);
            REQUIRE(svc);
            auto await = co_await (*svc).first->AwaitTask();
            REQUIRE(await.countConstructed == 1);
            co_return {};
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&dm = dm]() {
            INTERNAL_DEBUG("set");
            _evt->set();
            dm.getEventQueue().pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE(AwaitNoCopy::countConstructed == 1);
        REQUIRE(AwaitNoCopy::countDestructed == 3);
        REQUIRE(AwaitNoCopy::countMoved == 2);

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("stop in async start") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->createManager();

        std::thread t([&]() {
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<StopsInAsyncStartService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Dependency offline while starting service") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->createManager();
        uint64_t svcId{};

        std::thread t([&]() {
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            svcId = dm.createServiceManager<DependencyOfflineWhileStartingService, IDependencyOfflineWhileStartingService>()->getServiceId();
            dm.createServiceManager<UselessService, IUselessService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&dm = dm, svcId]() {
            auto services = dm.getStartedServices<IDependencyOfflineWhileStartingService>();

            REQUIRE(services.empty());

            auto svcs = dm.getAllServices();

#if defined(TEST_URING) || defined(TEST_SDEVENT)
            REQUIRE(svcs.size() == 5);
#else
            REQUIRE(svcs.size() == 4);
#endif

            REQUIRE(svcs.find(svcId)->second->getServiceState() == Ichor::ServiceState::STARTING);

            _evt->set();

            REQUIRE(svcs.find(svcId)->second->getServiceState() == Ichor::ServiceState::INJECTING);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&dm = dm, svcId]() {
            auto svcs = dm.getAllServices();

            REQUIRE(svcs.find(svcId)->second->getServiceState() == Ichor::ServiceState::INSTALLED);

            auto services = dm.getStartedServices<IDependencyOfflineWhileStartingService>();

            REQUIRE(services.empty());

            dm.getEventQueue().pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Dependency online while stopping service") {
#if defined(TEST_URING)
        auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
#else
        auto queue = std::make_unique<QIMPL>(500);
#endif
        auto &dm = queue->createManager();
        uint64_t svcId{};

        std::thread t([&]() {
#if defined(TEST_URING)
            REQUIRE(queue->createEventLoop());
#elif defined(TEST_SDEVENT)
            auto *loop = queue->createEventLoop();
            REQUIRE(loop);
#endif
            svcId = dm.createServiceManager<DependencyOnlineWhileStoppingService, IDependencyOnlineWhileStoppingService>()->getServiceId();
            dm.createServiceManager<UselessService, IUselessService>();
            queue->start(CaptureSigInt);
#if defined(TEST_SDEVENT)
            int r = sd_event_loop(loop);
            REQUIRE(r >= 0);
#endif
        });

        waitForRunning(dm);

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&dm = dm, svcId]() {
            auto services = dm.getStartedServices<IDependencyOnlineWhileStoppingService>();

            REQUIRE(services.empty());

            auto svcs = dm.getAllServices();

#if defined(TEST_URING) || defined(TEST_SDEVENT)
            REQUIRE(svcs.size() == 5);
#else
            REQUIRE(svcs.size() == 4);
#endif

            REQUIRE(svcs.find(svcId)->second->getServiceState() == Ichor::ServiceState::STOPPING);

            _evt->set();

            REQUIRE(svcs.find(svcId)->second->getServiceState() == Ichor::ServiceState::INSTALLED);
        });

        runForOrQueueEmpty(dm);

        queue->pushEvent<RunFunctionEvent>(0, [&dm = dm, svcId]() {
            auto services = dm.getStartedServices<IDependencyOnlineWhileStoppingService>();

            REQUIRE(services.empty());

            auto svcs = dm.getAllServices();

#if defined(TEST_URING) || defined(TEST_SDEVENT)
            REQUIRE(svcs.size() == 5);
#else
            REQUIRE(svcs.size() == 4);
#endif
            REQUIRE(svcs.find(svcId)->second->getServiceState() == Ichor::ServiceState::INSTALLED);

            dm.getEventQueue().pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    _evt.reset();
}
