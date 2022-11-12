#include "Common.h"
#include "TestServices/InterceptorService.h"
#include "TestServices/EventHandlerService.h"
#include "TestServices/AddInterceptorDuringEventHandlingService.h"
#include "TestEvents.h"
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/events/RunFunctionEvent.h>

using namespace Ichor;

bool AddInterceptorDuringEventHandlingService::_addedPreIntercept{};
bool AddInterceptorDuringEventHandlingService::_addedPostIntercept{};
bool AddInterceptorDuringEventHandlingService::_addedPreInterceptAll{};
bool AddInterceptorDuringEventHandlingService::_addedPostInterceptAll{};

TEST_CASE("Interceptor Tests") {

    SECTION("Intercept TestEvent unprocessed") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<InterceptorService<TestEvent>, IInterceptorService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.pushEvent<TestEvent>(0);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager& mng) -> AsyncGenerator<void> {
            auto services = mng.getStartedServices<IInterceptorService>();

            REQUIRE(services.size() == 1);

            auto &pre = services[0]->getPreinterceptedCounters();
            auto &post = services[0]->getPostinterceptedCounters();
            auto &un = services[0]->getUnprocessedInterceptedCounters();

            REQUIRE(pre.find(TestEvent::TYPE) != end(pre));
            REQUIRE(pre.find(TestEvent::TYPE)->second == 1);
            REQUIRE(post.find(TestEvent::TYPE) == end(post));
            REQUIRE(un.find(TestEvent::TYPE) != end(un));
            REQUIRE(un.find(TestEvent::TYPE)->second == 1);

            mng.pushEvent<QuitEvent>(0);

            co_return;
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Intercept TestEvent processed") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<InterceptorService<TestEvent>, IInterceptorService>();
            dm.createServiceManager<EventHandlerService<TestEvent>, IEventHandlerService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.pushEvent<TestEvent>(0);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager& mng) -> AsyncGenerator<void> {
            auto services = mng.getStartedServices<IInterceptorService>();

            REQUIRE(services.size() == 1);

            auto &pre = services[0]->getPreinterceptedCounters();
            auto &post = services[0]->getPostinterceptedCounters();
            auto &un = services[0]->getUnprocessedInterceptedCounters();

            REQUIRE(pre.find(TestEvent::TYPE) != end(pre));
            REQUIRE(pre.find(TestEvent::TYPE)->second == 1);
            REQUIRE(post.find(TestEvent::TYPE) != end(post));
            REQUIRE(post.find(TestEvent::TYPE)->second == 1);
            REQUIRE(un.find(TestEvent::TYPE) == end(un));

            mng.pushEvent<QuitEvent>(0);

            co_return;
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Intercept prevent handling") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<InterceptorService<TestEvent, false>, IInterceptorService>();
            dm.createServiceManager<EventHandlerService<TestEvent>, IEventHandlerService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.pushEvent<TestEvent>(0);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager& mng) -> AsyncGenerator<void> {
            auto interceptorServices = mng.getStartedServices<IInterceptorService>();
            auto eventHandlerServices = mng.getStartedServices<IEventHandlerService>();

            REQUIRE(interceptorServices.size() == 1);
            REQUIRE(eventHandlerServices.size() == 1);

            auto &pre = interceptorServices[0]->getPreinterceptedCounters();
            auto &post = interceptorServices[0]->getPostinterceptedCounters();
            auto &un = interceptorServices[0]->getUnprocessedInterceptedCounters();

            REQUIRE(pre.find(TestEvent::TYPE) != end(pre));
            REQUIRE(pre.find(TestEvent::TYPE)->second == 1);
            REQUIRE(post.find(TestEvent::TYPE) == end(post));
            REQUIRE(un.find(TestEvent::TYPE) != end(un));
            REQUIRE(un.find(TestEvent::TYPE)->second == 1);

            REQUIRE(eventHandlerServices[0]->getHandledEvents().empty());

            mng.pushEvent<QuitEvent>(0);

            co_return;
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Intercept all Events") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<InterceptorService<Event>, IInterceptorService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager& mng) -> AsyncGenerator<void> {
            auto services = mng.getStartedServices<IInterceptorService>();

            REQUIRE(services.size() == 1);

            auto &pre = services[0]->getPreinterceptedCounters();
            auto &post = services[0]->getPostinterceptedCounters();
            auto &un = services[0]->getUnprocessedInterceptedCounters();

            REQUIRE(pre.size() == 2);
            REQUIRE(pre.find(DependencyOnlineEvent::TYPE) != end(pre));
            REQUIRE(pre.find(DependencyOnlineEvent::TYPE)->second == 2);
            REQUIRE(pre.find(RunFunctionEvent::TYPE) != end(pre));
            REQUIRE(pre.find(RunFunctionEvent::TYPE)->second == 1);
            REQUIRE(post.find(DependencyOnlineEvent::TYPE) != end(post));
            REQUIRE(post.find(DependencyOnlineEvent::TYPE)->second == 2);
            REQUIRE(post.find(RunFunctionEvent::TYPE) == end(post));
            REQUIRE(un.find(DependencyOnlineEvent::TYPE) == end(un));
            REQUIRE(un.find(RunFunctionEvent::TYPE) == end(un));

            mng.pushEvent<QuitEvent>(0);

            co_return;
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Multiple interceptors") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<InterceptorService<Event>, IInterceptorService>();
            dm.createServiceManager<InterceptorService<Event>, IInterceptorService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager& mng) -> AsyncGenerator<void> {
            auto services = mng.getStartedServices<IInterceptorService>();

            REQUIRE(services.size() == 2);

            for(uint32_t i = 0; i < 2; i++) {
                auto &pre = services[i]->getPreinterceptedCounters();
                auto &post = services[i]->getPostinterceptedCounters();
                auto &un = services[i]->getUnprocessedInterceptedCounters();

                REQUIRE(pre.size() == 2);
                REQUIRE(pre.find(DependencyOnlineEvent::TYPE) != end(pre));
                REQUIRE(pre.find(DependencyOnlineEvent::TYPE)->second == 3);
                REQUIRE(pre.find(RunFunctionEvent::TYPE) != end(pre));
                REQUIRE(pre.find(RunFunctionEvent::TYPE)->second == 1);
                REQUIRE(post.find(DependencyOnlineEvent::TYPE) != end(post));
                REQUIRE(post.find(DependencyOnlineEvent::TYPE)->second == 3);
                REQUIRE(post.find(RunFunctionEvent::TYPE) == end(post));
                REQUIRE(un.find(DependencyOnlineEvent::TYPE) == end(un));
                REQUIRE(un.find(RunFunctionEvent::TYPE) == end(un));
            }

            mng.pushEvent<QuitEvent>(0);

            co_return;
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Add interceptors during intercept") {
        auto queue = std::make_unique<MultimapQueue>();
        auto &dm = queue->createManager();

        std::thread t([&]() {
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<AddInterceptorDuringEventHandlingService>();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        dm.runForOrQueueEmpty();

        dm.pushEvent<TestEvent>(0);

        dm.runForOrQueueEmpty();

        dm.pushEvent<QuitEvent>(0);

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }
}