#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include "TestServices/UselessService.h"
#include "TestServices/RegistrationCheckerService.h"
#include "TestServices/MultipleSeparateDependencyRequestsService.h"
#include "Common.h"

class ScopeFilter final {
public:
    explicit ScopeFilter(std::string _scope) : scope(std::move(_scope)) {}

    [[nodiscard]] bool matches(ILifecycleManager const &manager) const noexcept {
        if(manager.getDependencyRegistry() == nullptr) {
            fmt::print("ScopeFilter does not match, missing registry {}:{}\n", manager.serviceId(), manager.implementationName());
            return false;
        }

        for(auto const &[interfaceHash, depTuple] : manager.getDependencyRegistry()->_registrations) {
            if(interfaceHash != typeNameHash<IUselessService>()) {
                continue;
            }

            auto &props = std::get<tl::optional<Properties>>(depTuple);

            if(props) {
                for (auto const &[key, prop] : *props) {
                    fmt::print("depprop {} {}\n", key, prop);

                    if(key == "scope" && Ichor::any_cast<const std::string&>(prop) == scope) {
                        fmt::print("ScopeFilter matches {}:{}\n", manager.serviceId(), manager.implementationName());
                        return true;
                    }
                }
            }
        }

        fmt::print("ScopeFilter does not match {}:{}\n", manager.serviceId(), manager.implementationName());
        return false;
    }

    std::string scope;
};

TEST_CASE("DependencyManager") {
    SECTION("DependencyManager", "QuitOnQuitEvent") {
        auto queue = std::make_unique<PriorityQueue>();
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
        auto queue = std::make_unique<PriorityQueue>();
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

    SECTION("DependencyManager", "Check Multiple Registrations Different Properties") {
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();

        dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
        dm.createServiceManager<MultipleSeparateDependencyRequestsService>();
        dm.createServiceManager<UselessService, IUselessService>(Properties{{"Filter", Ichor::make_any<Filter>(ScopeFilter{"scope_one"})}});
        dm.createServiceManager<UselessService, IUselessService>(Properties{{"Filter", Ichor::make_any<Filter>(ScopeFilter{"scope_two"})}});
        dm.createServiceManager<UselessService, IUselessService>(Properties{{"Filter", Ichor::make_any<Filter>(ScopeFilter{"scope_three"})}});
        queue->start(CaptureSigInt);

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("DependencyManager", "Get services functions") {
        auto queue = std::make_unique<PriorityQueue>();
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

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            REQUIRE(dm.getServiceCount() == 5);

            auto svc = dm.getIService(uselessSvcId);
            REQUIRE(svc.has_value());
            REQUIRE(svc.value()->getServiceId() == uselessSvcId);
            REQUIRE(svc.value()->getServiceName() == typeName<UselessService>());

            auto loggerSvc = dm.getIService(loggerUuid);
            REQUIRE(loggerSvc.has_value());
            REQUIRE(loggerSvc.value()->getServiceGid() == loggerUuid);

            auto uselessSvcs = dm.getAllServicesOfType<IUselessService>();
            REQUIRE(uselessSvcs.size() == 2);

            auto startedSvc = dm.getStartedServices<IUselessService>();
            REQUIRE(startedSvc.size() == 2);

            queue->pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("DependencyManager", "RunFunctionEventAsync thread") {
        auto queue = std::make_unique<PriorityQueue>();
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

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            REQUIRE(Ichor::Detail::_local_dm == &dm);
            REQUIRE(testThreadId != std::this_thread::get_id());
            REQUIRE(dmThreadId == std::this_thread::get_id());
            co_await evt;
            REQUIRE(Ichor::Detail::_local_dm == &dm);
            REQUIRE(testThreadId != std::this_thread::get_id());
            REQUIRE(dmThreadId == std::this_thread::get_id());
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        dm.runForOrQueueEmpty();

        REQUIRE(Ichor::Detail::_local_dm == nullptr);

        queue->pushEvent<RunFunctionEvent>(0, [&]() {
            REQUIRE(Ichor::Detail::_local_dm == &dm);
            REQUIRE(testThreadId != std::this_thread::get_id());
            REQUIRE(dmThreadId == std::this_thread::get_id());
            evt.set();
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }
}
