#include <catch2/catch_test_macros.hpp>
#include <ichor/event_queues/PriorityQueue.h>
#include "TestServices/UselessService.h"

#include <ichor/DependencyManager.h>
#include <ichor/CommunicationChannel.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/events/Event.h>
#include <ichor/events/RunFunctionEvent.h>
#include <iostream>
#include <thread>

using namespace Ichor;

struct IMyService {}; // the interface

struct MyService final : public IMyService, public Ichor::AdvancedService<MyService> {
    MyService() = default;
}; // a minimal implementation

struct IMyDependencyService {};

struct MyDependencyService final : public IMyDependencyService, public Ichor::AdvancedService<MyDependencyService> {
    MyDependencyService(Ichor::DependencyRegister &reg, Ichor::Properties props) : Ichor::AdvancedService<MyDependencyService>(std::move(props)) {
        reg.registerDependency<IMyService>(this, DependencyFlags::REQUIRED);
    }
    ~MyDependencyService() final = default;

    void addDependencyInstance(IMyService&, Ichor::IService&) {
        std::cout << "Got MyService!" << std::endl;
    }
    void removeDependencyInstance(IMyService&, Ichor::IService&) {
        std::cout << "Removed MyService!" << std::endl;
    }
};

struct IMyTimerService {};

struct MyTimerService final : public IMyTimerService {
    MyTimerService(ITimerFactory *factory) {
        auto &timer = factory->createTimer();
        timer.setChronoInterval(std::chrono::seconds(1));
        timer.setCallback([]() {
            fmt::print("Timer fired\n");
        });
        timer.startTimer();
    }

    Ichor::EventHandlerRegistration _timerEventRegistration{};
};

int example() {
    auto queue = std::make_unique<PriorityQueue>();
    auto &dm = queue->createManager();
    dm.createServiceManager<MyService, IMyService>();
    dm.createServiceManager<MyDependencyService, IMyDependencyService>();
    dm.createServiceManager<MyTimerService, IMyTimerService>(); // Add this
    queue->start(CaptureSigInt);

    return 0;
}

struct ServiceWithoutInterface final : public Ichor::AdvancedService<ServiceWithoutInterface> {
    ServiceWithoutInterface() = default;
};

struct MyInterceptorService final : public Ichor::AdvancedService<MyInterceptorService> {
    Task<tl::expected<void, Ichor::StartError>> start() final {
        _interceptor = GetThreadLocalManager().template registerEventInterceptor<Ichor::RunFunctionEventAsync>(this, this); // Can change TimerEvent to just Event if you want to intercept *all* events
        co_return {};
    }

    Task<void> stop() final {
        _interceptor.reset();
        co_return;
    }

    bool preInterceptEvent(Ichor::RunFunctionEventAsync const &) {
        return AllowOthersHandling; //PreventOthersHandling if this event should be discarded
    }

    void postInterceptEvent(Ichor::RunFunctionEventAsync const &, bool processed) {
        // Can use this to track how long the processing took
    }

    Ichor::EventInterceptorRegistration _interceptor{};
};

int communication() {
    Ichor::CommunicationChannel channel{};
    auto queueOne = std::make_unique<PriorityQueue>();
    auto &dmOne = queueOne->createManager(); // ID = 0
    auto queueTwo = std::make_unique<PriorityQueue>();
    auto &dmTwo = queueTwo->createManager(); // ID = 1

    // Register the manager to the channel
    channel.addManager(&dmOne);
    channel.addManager(&dmTwo);

    std::thread t1([&] {
        // your services here
        queueOne->start(CaptureSigInt);
    });

    std::thread t2([&] {
        // your services here
        queueTwo->start(CaptureSigInt);
    });

    t1.join();
    t2.join();

    return 0;
}
