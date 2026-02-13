#include "Common.h"
#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <ichor/Filter.h>
#include "TestServices/UselessService.h"
#include "TestServices/RegistrationCheckerService.h"
#include "TestServices/MultipleSeparateDependencyRequestsService.h"

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

                    if(key == "scope" && Ichor::v1::any_cast<const std::string&>(prop) == scope) {
                        fmt::print("ScopeFilter matches {}:{}\n", manager.serviceId(), manager.implementationName());
                        return true;
                    }
                }
            }
        }

        fmt::print("ScopeFilter does not match {}:{}\n", manager.serviceId(), manager.implementationName());
        return false;
    }

    [[nodiscard]] std::string getDescription() const noexcept {
        std::string s;
        fmt::format_to(std::back_inserter(s), "ScopeFilter {}", scope);
        return s;
    }

    std::string scope;
};

TEST_CASE("DependencyManager: QuitOnQuitEvent") {
    auto queue = std::make_unique<PriorityQueue>();
    auto &dm = queue->createManager();

    std::thread t([&]() {
        dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
        dm.createServiceManager<UselessService>();
        queue->start(CaptureSigInt);
    });

    runForOrQueueEmpty(dm);

    REQUIRE(dm.isRunning());

    queue->pushEvent<QuitEvent>(ServiceIdType{0});

    t.join();

    REQUIRE_FALSE(dm.isRunning());
}

TEST_CASE("DependencyManager: Check Registrations") {
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

TEST_CASE("DependencyManager: Check Multiple Registrations Different Properties") {
    auto queue = std::make_unique<PriorityQueue>();
    auto &dm = queue->createManager();

    dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
    dm.createServiceManager<MultipleSeparateDependencyRequestsService>();
    dm.createServiceManager<UselessService, IUselessService>(Properties{{"Filter", Ichor::v1::make_any<Filter>(ScopeFilter{"scope_one"})}});
    dm.createServiceManager<UselessService, IUselessService>(Properties{{"Filter", Ichor::v1::make_any<Filter>(ScopeFilter{"scope_two"})}});
    dm.createServiceManager<UselessService, IUselessService>(Properties{{"Filter", Ichor::v1::make_any<Filter>(ScopeFilter{"scope_three"})}});
    queue->start(CaptureSigInt);

    REQUIRE_FALSE(dm.isRunning());
}

TEST_CASE("DependencyManager: Get services functions") {
    auto queue = std::make_unique<PriorityQueue>();
    auto &dm = queue->createManager();
    ServiceIdType uselessSvcId{};
    sole::uuid loggerUuid{};

    std::thread t([&]() {
        loggerUuid = dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>()->getServiceGid();
        uselessSvcId = dm.createServiceManager<UselessService, IUselessService>()->getServiceId();
        dm.createServiceManager<Ichor::UselessService, Ichor::IUselessService>();
        queue->start(CaptureSigInt);
    });

    waitForRunning(dm);

    runForOrQueueEmpty(dm);

    queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
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

        queue->pushEvent<QuitEvent>(ServiceIdType{0});
    });

    t.join();

    REQUIRE_FALSE(dm.isRunning());
}

TEST_CASE("DependencyManager: RunFunctionEventAsync thread") {
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

    runForOrQueueEmpty(dm);

    REQUIRE(dm.isRunning());

    REQUIRE(Ichor::Detail::_local_dm == nullptr);

    AsyncManualResetEvent evt;

    queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, [&]() -> AsyncGenerator<IchorBehaviour> {
        REQUIRE(Ichor::Detail::_local_dm == &dm);
        REQUIRE(testThreadId != std::this_thread::get_id());
        REQUIRE(dmThreadId == std::this_thread::get_id());
        co_await evt;
        REQUIRE(Ichor::Detail::_local_dm == &dm);
        REQUIRE(testThreadId != std::this_thread::get_id());
        REQUIRE(dmThreadId == std::this_thread::get_id());
        queue->pushEvent<QuitEvent>(ServiceIdType{0});
        co_return {};
    });

    runForOrQueueEmpty(dm);

    REQUIRE(Ichor::Detail::_local_dm == nullptr);

    queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
        REQUIRE(Ichor::Detail::_local_dm == &dm);
        REQUIRE(testThreadId != std::this_thread::get_id());
        REQUIRE(dmThreadId == std::this_thread::get_id());
        evt.set();
    });

    t.join();

    REQUIRE_FALSE(dm.isRunning());
}

TEST_CASE("DependencyManager: Registration classes reset") {
    auto queue = std::make_unique<PriorityQueue>();
    auto &dm = queue->createManager();
    std::thread t([&]() {
        dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
        dm.createServiceManager<UselessService>();
        queue->start(CaptureSigInt);
    });

    runForOrQueueEmpty(dm);

    REQUIRE(dm.isRunning());

    queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
        REQUIRE(queue->size() == 1);
        {
            EventHandlerRegistration reg{CallbackKey{ServiceIdType{234}, 345}, 123};
            REQUIRE(queue->size() == 1);
            reg.reset();
            REQUIRE(queue->size() == 2);
            reg.reset();
            REQUIRE(queue->size() == 2);
        }
        REQUIRE(queue->size() == 2);
        {
            EventInterceptorRegistration reg{ServiceIdType{234}, 345, 456, 567};
            REQUIRE(queue->size() == 2);
            reg.reset();
            REQUIRE(queue->size() == 3);
            reg.reset();
            REQUIRE(queue->size() == 3);
        }
        REQUIRE(queue->size() == 3);
        {
            DependencyTrackerRegistration reg{ServiceIdType{234}, 345, 123};
            REQUIRE(queue->size() == 3);
            reg.reset();
            REQUIRE(queue->size() == 4);
            reg.reset();
            REQUIRE(queue->size() == 4);
        }
        REQUIRE(queue->size() == 4);

        queue->pushEvent<QuitEvent>(ServiceIdType{0});
    });

    t.join();
}

TEST_CASE("DependencyManager: Registration classes move assign") {
    auto queue = std::make_unique<PriorityQueue>();
    auto &dm = queue->createManager();
    std::thread t([&]() {
        dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
        dm.createServiceManager<UselessService>();
        queue->start(CaptureSigInt);
    });

    runForOrQueueEmpty(dm);

    REQUIRE(dm.isRunning());

    queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
        REQUIRE(queue->size() == 1);
        {
            EventHandlerRegistration reg{CallbackKey{ServiceIdType{234}, 345}, 123};
            reg = EventHandlerRegistration{CallbackKey{ServiceIdType{345}, 456}, 234};
            REQUIRE(queue->size() == 2);
        }
        REQUIRE(queue->size() == 3);
        {
            EventInterceptorRegistration reg{ServiceIdType{234}, 345, 456, 567};
            reg = EventInterceptorRegistration{ServiceIdType{432}, 543, 654, 765};
            REQUIRE(queue->size() == 4);
        }
        REQUIRE(queue->size() == 5);
        {
            DependencyTrackerRegistration reg{ServiceIdType{234}, 345, 123};
            reg = DependencyTrackerRegistration{ServiceIdType{345}, 456, 234};
            REQUIRE(queue->size() == 6);
        }
        REQUIRE(queue->size() == 7);

        queue->pushEvent<QuitEvent>(ServiceIdType{0});
    });

    t.join();
}

TEST_CASE("DependencyManager: Registration classes move construct") {
    auto queue = std::make_unique<PriorityQueue>();
    auto &dm = queue->createManager();
    std::thread t([&]() {
        dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
        dm.createServiceManager<UselessService>();
        queue->start(CaptureSigInt);
    });

    runForOrQueueEmpty(dm);

    REQUIRE(dm.isRunning());

    queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
        REQUIRE(queue->size() == 1);
        {
            EventHandlerRegistration reg{EventHandlerRegistration{CallbackKey{ServiceIdType{345}, 456}, 234}};
        }
        REQUIRE(queue->size() == 2);
        {
            EventInterceptorRegistration reg{EventInterceptorRegistration{ServiceIdType{234}, 345, 456, 567}};
        }
        REQUIRE(queue->size() == 3);
        {
            DependencyTrackerRegistration reg{DependencyTrackerRegistration{ServiceIdType{345}, 456, 234}};
        }
        REQUIRE(queue->size() == 4);

        queue->pushEvent<QuitEvent>(ServiceIdType{0});
    });

    t.join();
}

TEST_CASE("DependencyManager: Global Interceptor") {
    auto queue = std::make_unique<PriorityQueue>();
    auto &dm = queue->createManager();
    uint64_t preIntercepted{};
    uint64_t postIntercepted{};
    auto evtInterceptor = dm.registerGlobalEventInterceptor<StartServiceEvent>([&](StartServiceEvent const &evt) -> bool {
        preIntercepted++;
        return true;
    }, [&](StartServiceEvent const &evt, bool processed) {
        postIntercepted++;
    });
    std::thread t([&]() {
        dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
        dm.createServiceManager<UselessService>();
        queue->start(CaptureSigInt);
    });

    runForOrQueueEmpty(dm);

    REQUIRE(dm.isRunning());

    REQUIRE(preIntercepted == 2);
    REQUIRE(postIntercepted == 2);

    queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
        evtInterceptor.reset();
        queue->pushEvent<QuitEvent>(ServiceIdType{0});
    });

    t.join();
}
