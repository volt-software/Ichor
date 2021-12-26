#include "catch2/catch_test_macros.hpp"
#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/NullFrameworkLogger.h>
#include "InterceptorService.h"
#include "EventHandlerService.h"
#include "TestEvents.h"
#include "spdlog/sinks/stdout_color_sinks.h"

using namespace Ichor;

TEST_CASE("Interceptor Tests") {

#if __has_include(<spdlog/spdlog.h>)
    //default logger is disabled in cmake
    if(spdlog::default_logger_raw() == nullptr) {
        auto new_logger = spdlog::stdout_color_st("new_default_logger");
        spdlog::set_default_logger(new_logger);
    }
#endif

    SECTION("Intercept TestEvent unprocessed") {
        Ichor::DependencyManager dm{};

        std::thread t([&]() {
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<InterceptorService<TestEvent>, IInterceptorService>();
            dm.start();
        });

        dm.pushEvent<TestEvent>(0);

        dm.waitForEmptyQueue();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager* mng){
            auto services = mng->getStartedServices<IInterceptorService>();

            REQUIRE(services.size() == 1);

            auto &pre = services[0]->getPreinterceptedCounters();
            auto &post = services[0]->getPostinterceptedCounters();
            auto &un = services[0]->getUnprocessedInterceptedCounters();

            REQUIRE(pre.find(TestEvent::TYPE) != end(pre));
            REQUIRE(pre.find(TestEvent::TYPE)->second == 1);
            REQUIRE(post.find(TestEvent::TYPE) == end(pre));
            REQUIRE(un.find(TestEvent::TYPE) != end(pre));
            REQUIRE(un.find(TestEvent::TYPE)->second == 1);

            mng->pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Intercept TestEvent processed") {
        Ichor::DependencyManager dm{};

        std::thread t([&]() {
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<InterceptorService<TestEvent>, IInterceptorService>();
            dm.createServiceManager<EventHandlerService<TestEvent>, IEventHandlerService>();
            dm.start();
        });

        dm.pushEvent<TestEvent>(0);

        dm.waitForEmptyQueue();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager* mng){
            auto services = mng->getStartedServices<IInterceptorService>();

            REQUIRE(services.size() == 1);

            auto &pre = services[0]->getPreinterceptedCounters();
            auto &post = services[0]->getPostinterceptedCounters();
            auto &un = services[0]->getUnprocessedInterceptedCounters();

            REQUIRE(pre.find(TestEvent::TYPE) != end(pre));
            REQUIRE(pre.find(TestEvent::TYPE)->second == 1);
            REQUIRE(post.find(TestEvent::TYPE) != end(pre));
            REQUIRE(post.find(TestEvent::TYPE)->second == 1);
            REQUIRE(un.find(TestEvent::TYPE) == end(pre));

            mng->pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Intercept prevent handling") {
        Ichor::DependencyManager dm{};

        std::thread t([&]() {
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<InterceptorService<TestEvent, false>, IInterceptorService>();
            dm.createServiceManager<EventHandlerService<TestEvent>, IEventHandlerService>();
            dm.start();
        });

        dm.pushEvent<TestEvent>(0);

        dm.waitForEmptyQueue();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager* mng){
            auto interceptorServices = mng->getStartedServices<IInterceptorService>();
            auto eventHandlerServices = mng->getStartedServices<IEventHandlerService>();

            REQUIRE(interceptorServices.size() == 1);
            REQUIRE(eventHandlerServices.size() == 1);

            auto &pre = interceptorServices[0]->getPreinterceptedCounters();
            auto &post = interceptorServices[0]->getPostinterceptedCounters();
            auto &un = interceptorServices[0]->getUnprocessedInterceptedCounters();

            REQUIRE(pre.find(TestEvent::TYPE) != end(pre));
            REQUIRE(pre.find(TestEvent::TYPE)->second == 1);
            REQUIRE(post.find(TestEvent::TYPE) == end(pre));
            REQUIRE(un.find(TestEvent::TYPE) != end(pre));
            REQUIRE(un.find(TestEvent::TYPE)->second == 1);

            REQUIRE(eventHandlerServices[0]->getHandledEvents().empty());

            mng->pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Intercept all Events") {
        Ichor::DependencyManager dm{};

        std::thread t([&]() {
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<InterceptorService<Event>, IInterceptorService>();
            dm.start();
        });

        dm.waitForEmptyQueue();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager* mng){
            auto services = mng->getStartedServices<IInterceptorService>();

            REQUIRE(services.size() == 1);

            auto &pre = services[0]->getPreinterceptedCounters();
            auto &post = services[0]->getPostinterceptedCounters();
            auto &un = services[0]->getUnprocessedInterceptedCounters();

            REQUIRE(pre.size() == 2);
            REQUIRE(pre.find(DependencyOnlineEvent::TYPE) != end(pre));
            REQUIRE(pre.find(DependencyOnlineEvent::TYPE)->second == 2);
            REQUIRE(pre.find(RunFunctionEvent::TYPE) != end(pre));
            REQUIRE(pre.find(RunFunctionEvent::TYPE)->second == 1);
            REQUIRE(post.find(DependencyOnlineEvent::TYPE) != end(pre));
            REQUIRE(post.find(DependencyOnlineEvent::TYPE)->second == 2);
            REQUIRE(post.find(RunFunctionEvent::TYPE) == end(pre));
            REQUIRE(un.find(DependencyOnlineEvent::TYPE) == end(pre));
            REQUIRE(un.find(RunFunctionEvent::TYPE) == end(pre));

            mng->pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }

    SECTION("Multiple interceptors") {
        Ichor::DependencyManager dm{};

        std::thread t([&]() {
            dm.createServiceManager<NullFrameworkLogger, IFrameworkLogger>();
            dm.createServiceManager<InterceptorService<Event>, IInterceptorService>();
            dm.createServiceManager<InterceptorService<Event>, IInterceptorService>();
            dm.start();
        });

        dm.waitForEmptyQueue();

        dm.pushEvent<RunFunctionEvent>(0, [](DependencyManager* mng){
            auto services = mng->getStartedServices<IInterceptorService>();

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
                REQUIRE(post.find(DependencyOnlineEvent::TYPE) != end(pre));
                REQUIRE(post.find(DependencyOnlineEvent::TYPE)->second == 3);
                REQUIRE(post.find(RunFunctionEvent::TYPE) == end(pre));
                REQUIRE(un.find(DependencyOnlineEvent::TYPE) == end(pre));
                REQUIRE(un.find(RunFunctionEvent::TYPE) == end(pre));
            }

            mng->pushEvent<QuitEvent>(0);
        });

        t.join();

        REQUIRE_FALSE(dm.isRunning());
    }
}