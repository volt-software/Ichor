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

        dm.pushEvent<RunFunctionEventAsync>(0, [](DependencyManager& mng) -> AsyncGenerator<IchorBehaviour>{
            auto services = mng.getStartedServices<IGeneratorService>();

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

            mng.pushEvent<QuitEvent>(0);
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

        dm.pushEvent<RunFunctionEventAsync>(0, [](DependencyManager& mng) -> AsyncGenerator<IchorBehaviour> {
            auto services = mng.getStartedServices<IAwaitService>();

            REQUIRE(services.size() == 1);

            INTERNAL_DEBUG("before");

            co_await services[0]->await_something();

            INTERNAL_DEBUG("after");

            co_yield {};

            INTERNAL_DEBUG("quit");

            mng.pushEvent<QuitEvent>(0);

            INTERNAL_DEBUG("after2");

            co_return {};
        });

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager& mng) {
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

        dm.pushEvent<AwaitEvent>(0);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager& mng) {
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

        dm.pushEvent<RunFunctionEvent>(0, [&](DependencyManager& mng) {
            INTERNAL_DEBUG("set");
            _autoEvt->set_all();
            REQUIRE(svc->count == 2);
            mng.pushEvent<QuitEvent>(0);
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
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager& mng) {
            INTERNAL_DEBUG("set");
            _evt->set();
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("co_await user defined struct") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        AwaitReturnService *svc{};

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            svc = dm.createServiceManager<AwaitReturnService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEventAsync>(0, [&](DependencyManager& mng) -> AsyncGenerator<IchorBehaviour> {
            auto &await = *co_await svc->Await().begin();
            co_return {};
        });

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager& mng) {
            INTERNAL_DEBUG("set");
            _evt->set();
            mng.pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE(AwaitNoCopy::countConstructed == 1);
        REQUIRE(AwaitNoCopy::countDestructed == 2);
        REQUIRE(AwaitNoCopy::countMoved == 1);

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

        dm.pushEvent<RunFunctionEvent>(0, [svcId](DependencyManager& mng) {
            auto services = mng.getStartedServices<IDependencyOfflineWhileStartingService>();

            REQUIRE(services.empty());

            auto svcs = mng.getServiceInfo();

            REQUIRE(svcs.size() == 2);

            REQUIRE(svcs[svcId]->getServiceState() == Ichor::ServiceState::STARTING);

            _evt->set();

            REQUIRE(svcs[svcId]->getServiceState() == Ichor::ServiceState::INJECTING);
        });

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [svcId](DependencyManager& mng) {
            auto svcs = mng.getServiceInfo();

            REQUIRE(svcs[svcId]->getServiceState() == Ichor::ServiceState::INSTALLED);

            auto services = mng.getStartedServices<IDependencyOfflineWhileStartingService>();

            REQUIRE(services.empty());

            mng.pushEvent<QuitEvent>(0);
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

        dm.pushEvent<RunFunctionEvent>(0, [svcId](DependencyManager& mng) {
            auto services = mng.getStartedServices<IDependencyOnlineWhileStoppingService>();

            REQUIRE(services.empty());

            auto svcs = mng.getServiceInfo();

            REQUIRE(svcs.size() == 2);

            REQUIRE(svcs[svcId]->getServiceState() == Ichor::ServiceState::STOPPING);

            _evt->set();

            REQUIRE(svcs[svcId]->getServiceState() == Ichor::ServiceState::INSTALLED);
        });

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [svcId](DependencyManager& mng) {
            auto services = mng.getStartedServices<IDependencyOnlineWhileStoppingService>();

            REQUIRE(services.empty());

            auto svcs = mng.getServiceInfo();

            REQUIRE(svcs.size() == 2);
            REQUIRE(svcs[svcId]->getServiceState() == Ichor::ServiceState::INSTALLED);

            mng.pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    _evt.reset();
}
