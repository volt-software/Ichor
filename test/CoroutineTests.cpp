#include "Common.h"
#include "TestServices/GeneratorService.h"
#include "TestServices/AwaitService.h"
#include "TestServices/MultipleAwaitersService.h"
#include "TestServices/AsyncUsingTimerService.h"
#include "TestServices/AwaitReturnService.h"
#include "TestServices/StopsInAsyncStartService.h"
#include "TestServices/DependencyOfflineWhileStartingService.h"
#include "TestServices/DependencyOnlineWhileStoppingService.h"
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/services/timer/TimerFactoryFactory.h>
#include <memory>

std::unique_ptr<Ichor::AsyncManualResetEvent> _evt;
std::unique_ptr<Ichor::AsyncAutoResetEvent> _autoEvt;
uint64_t AwaitNoCopy::countConstructed{};
uint64_t AwaitNoCopy::countDestructed{};
uint64_t AwaitNoCopy::countMoved{};

TEST_CASE("CoroutineTests") {
    _evt = std::make_unique<Ichor::AsyncManualResetEvent>();

    SECTION("Required dependencies") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<GeneratorService, IGeneratorService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

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
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<AwaitService, IAwaitService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

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

        dm.runForOrQueueEmpty();

        queue->pushEvent<RunFunctionEvent>(0, []() {
            INTERNAL_DEBUG("set");
            _evt->set();
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("co_await in event handler") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<EventAwaitService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        queue->pushEvent<AwaitEvent>(0);

        dm.runForOrQueueEmpty();

        queue->pushEvent<RunFunctionEvent>(0, []() {
            INTERNAL_DEBUG("set");
            _evt->set();
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("multiple awaiters in event handler") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        _autoEvt = std::make_unique<Ichor::AsyncAutoResetEvent>();
        MultipleAwaitService *svc{};

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            svc = dm.createServiceManager<MultipleAwaitService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            INTERNAL_DEBUG("set");
            _autoEvt->set_all();
            REQUIRE(svc->count == 2);
            dm.getEventQueue().pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("co_await in timer service") {
        fmt::print("co_await in timer service\n");
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<AwaitService, IAwaitService>();
            dm.createServiceManager<AsyncUsingTimerService>();
            dm.createServiceManager<TimerFactoryFactory>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        queue->pushEvent<RunFunctionEvent>(0, []() {
            INTERNAL_DEBUG("set");
            _evt->set();
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("co_await user defined struct generator") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        AwaitReturnService *svc{};
        AwaitNoCopy::countConstructed = 0;
        AwaitNoCopy::countDestructed = 0;
        AwaitNoCopy::countMoved = 0;

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            svc = dm.createServiceManager<AwaitReturnService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            auto &await = *co_await svc->Await().begin();
            co_return {};
        });

        dm.runForOrQueueEmpty();

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
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        AwaitReturnService *svc{};
        AwaitNoCopy::countConstructed = 0;
        AwaitNoCopy::countDestructed = 0;
        AwaitNoCopy::countMoved = 0;

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            svc = dm.createServiceManager<AwaitReturnService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            auto await = co_await svc->AwaitTask();
            co_return {};
        });

        dm.runForOrQueueEmpty();

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
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<StopsInAsyncStartService>();
            queue->start(CaptureSigInt);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Dependency offline while starting service") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        uint64_t svcId{};

        std::thread t([&]() {
            svcId = dm.createServiceManager<DependencyOfflineWhileStartingService, IDependencyOfflineWhileStartingService>()->getServiceId();
            dm.createServiceManager<UselessService, IUselessService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        queue->pushEvent<RunFunctionEvent>(0, [&dm = dm, svcId]() {
            auto services = dm.getStartedServices<IDependencyOfflineWhileStartingService>();

            REQUIRE(services.empty());

            auto svcs = dm.getServiceInfo();

            REQUIRE(svcs.size() == 4);

            REQUIRE(svcs[svcId]->getServiceState() == Ichor::ServiceState::STARTING);

            _evt->set();

            REQUIRE(svcs[svcId]->getServiceState() == Ichor::ServiceState::INJECTING);
        });

        dm.runForOrQueueEmpty();

        queue->pushEvent<RunFunctionEvent>(0, [&dm = dm, svcId]() {
            auto svcs = dm.getServiceInfo();

            REQUIRE(svcs[svcId]->getServiceState() == Ichor::ServiceState::INSTALLED);

            auto services = dm.getStartedServices<IDependencyOfflineWhileStartingService>();

            REQUIRE(services.empty());

            dm.getEventQueue().pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Dependency online while stopping service") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        uint64_t svcId{};

        std::thread t([&]() {
            svcId = dm.createServiceManager<DependencyOnlineWhileStoppingService, IDependencyOnlineWhileStoppingService>()->getServiceId();
            dm.createServiceManager<UselessService, IUselessService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        queue->pushEvent<RunFunctionEvent>(0, [&dm = dm, svcId]() {
            auto services = dm.getStartedServices<IDependencyOnlineWhileStoppingService>();

            REQUIRE(services.empty());

            auto svcs = dm.getServiceInfo();

            REQUIRE(svcs.size() == 4);

            REQUIRE(svcs[svcId]->getServiceState() == Ichor::ServiceState::STOPPING);

            _evt->set();

            REQUIRE(svcs[svcId]->getServiceState() == Ichor::ServiceState::INSTALLED);
        });

        dm.runForOrQueueEmpty();

        queue->pushEvent<RunFunctionEvent>(0, [&dm = dm, svcId]() {
            auto services = dm.getStartedServices<IDependencyOnlineWhileStoppingService>();

            REQUIRE(services.empty());

            auto svcs = dm.getServiceInfo();

            REQUIRE(svcs.size() == 4);
            REQUIRE(svcs[svcId]->getServiceState() == Ichor::ServiceState::INSTALLED);

            dm.getEventQueue().pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    _evt.reset();
}
