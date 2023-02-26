#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include "TestServices/UselessService.h"
#include "TestServices/RegistrationCheckerService.h"
#include "Common.h"

TEST_CASE("DependencyManager") {
    SECTION("DependencyManager", "QuitOnQuitEvent") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService>();
            queue->start(CaptureSigInt);
        });

        dm.runForOrQueueEmpty();

        REQUIRE(dm.isRunning());

        queue->pushEvent<QuitEvent>(0);

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("DependencyManager", "Check Registrations") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<RegistrationCheckerService>();
            dm.createServiceManager<UselessService, IUselessService>();
            dm.createServiceManager<Ichor::UselessService, Ichor::IUselessService>();
            queue->start(CaptureSigInt);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("DependencyManager", "Get services functions") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        uint64_t uselessSvcId{};
        sole::uuid loggerUuid{};

        std::thread t([&]() {
            loggerUuid = dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>()->getServiceGid();
            uselessSvcId = dm.createServiceManager<UselessService, IUselessService>()->getServiceId();
            dm.createServiceManager<Ichor::UselessService, Ichor::IUselessService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        queue->pushEvent<RunFunctionEvent>(0, [&](DependencyManager &_dm) {
            REQUIRE(_dm.getServiceCount() == 3);

            auto svc = _dm.getService(uselessSvcId);
            REQUIRE(svc.has_value());
            REQUIRE(svc.value()->getServiceId() == uselessSvcId);
            REQUIRE(svc.value()->getServiceName() == typeName<UselessService>());

            auto loggerSvc = _dm.getService(loggerUuid);
            REQUIRE(loggerSvc.has_value());
            REQUIRE(loggerSvc.value()->getServiceGid() == loggerUuid);

            auto uselessSvcs = _dm.getAllServicesOfType<IUselessService>();
            REQUIRE(uselessSvcs.size() == 2);

            auto startedSvc = _dm.getStartedServices<IUselessService>();
            REQUIRE(startedSvc.size() == 2);

            queue->pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("DependencyManager", "RunFunctionEventAsync thread") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();
        std::thread::id testThreadId = std::this_thread::get_id();
        std::thread::id dmThreadId;

        REQUIRE(Ichor::Detail::_local_dm == nullptr);

        std::thread t([&]() {
            dmThreadId = std::this_thread::get_id();
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<UselessService>();
            queue->start(CaptureSigInt);
        });

        dm.runForOrQueueEmpty();

        REQUIRE(dm.isRunning());

        REQUIRE(Ichor::Detail::_local_dm == nullptr);

        AsyncManualResetEvent evt;

        queue->pushEvent<RunFunctionEventAsync>(0, [&](DependencyManager &_dm) -> AsyncGenerator<IchorBehaviour> {
            REQUIRE(Ichor::Detail::_local_dm == &_dm);
            REQUIRE(Ichor::Detail::_local_dm == &dm);
            REQUIRE(testThreadId != std::this_thread::get_id());
            REQUIRE(dmThreadId == std::this_thread::get_id());
            co_await evt;
            REQUIRE(Ichor::Detail::_local_dm == &_dm);
            REQUIRE(Ichor::Detail::_local_dm == &dm);
            REQUIRE(testThreadId != std::this_thread::get_id());
            REQUIRE(dmThreadId == std::this_thread::get_id());
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        dm.runForOrQueueEmpty();

        REQUIRE(Ichor::Detail::_local_dm == nullptr);

        queue->pushEvent<RunFunctionEvent>(0, [&](DependencyManager &_dm) {
            REQUIRE(Ichor::Detail::_local_dm == &_dm);
            REQUIRE(Ichor::Detail::_local_dm == &dm);
            REQUIRE(testThreadId != std::this_thread::get_id());
            REQUIRE(dmThreadId == std::this_thread::get_id());
            evt.set();
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }
}
