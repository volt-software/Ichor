#include <catch2/catch_test_macros.hpp>
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/optional_bundles/logging_bundle/NullFrameworkLogger.h>
#include "TestServices/UselessService.h"

#include <ichor/DependencyManager.h>
#include <ichor/CommunicationChannel.h>
#include <ichor/optional_bundles/logging_bundle/CoutFrameworkLogger.h>
#include <ichor/optional_bundles/timer_bundle/TimerService.h>
#include <ichor/Events.h>
#include <iostream>

struct IMyService {}; // the interface

struct MyService final : public IMyService, public Ichor::Service<MyService> {
    MyService() = default;
}; // a minimal implementation

struct IMyDependencyService {};

struct MyDependencyService final : public IMyDependencyService, public Ichor::Service<MyDependencyService> {
    MyDependencyService(Ichor::DependencyRegister &reg, Ichor::Properties props, Ichor::DependencyManager *mng) : Ichor::Service<MyDependencyService>(std::move(props), mng) {
        reg.registerDependency<IMyService>(this, true);
    }
    ~MyDependencyService() final = default;

    void addDependencyInstance(IMyService *, Ichor::IService *) {
        std::cout << "Got MyService!" << std::endl;
    }
    void removeDependencyInstance(IMyService *, Ichor::IService *) {
        std::cout << "Removed MyService!" << std::endl;
    }
};

struct IMyTimerService {};

struct MyTimerService final : public IMyTimerService, public Ichor::Service<MyTimerService> {
    MyTimerService() = default;
    StartBehaviour start() final {
        auto timer = getManager()->createServiceManager<Ichor::Timer, Ichor::ITimer>();
        timer->setChronoInterval(std::chrono::seconds(1));
        _timerEventRegistration = getManager()->registerEventHandler<Ichor::TimerEvent>(this, timer->getServiceId());
        timer->startTimer();
        return StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        _timerEventRegistration.reset();
        return StartBehaviour::SUCCEEDED;
    }

    Ichor::Generator<bool> handleEvent(Ichor::TimerEvent const * const) {
        co_return (bool)PreventOthersHandling;
    }

    Ichor::EventHandlerRegistration _timerEventRegistration{};
};

int example() {
    Ichor::DependencyManager dm{std::make_unique<MultimapQueue>()};
    dm.createServiceManager<Ichor::CoutFrameworkLogger, Ichor::IFrameworkLogger>();
    dm.createServiceManager<MyService, IMyService>();
    dm.createServiceManager<MyDependencyService, IMyDependencyService>();
    dm.createServiceManager<MyTimerService, IMyTimerService>(); // Add this
    dm.start();

    return 0;
}

struct ServiceWithoutInterface final : public Ichor::Service<ServiceWithoutInterface> {
    ServiceWithoutInterface() = default;
};

struct MyInterceptorService final : public Ichor::Service<MyInterceptorService> {
    StartBehaviour start() final {
        _interceptor = this->getManager()->template registerEventInterceptor<Ichor::TimerEvent>(this); // Can change TimerEvent to just Event if you want to intercept *all* events
        return StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        _interceptor.reset();
        return StartBehaviour::SUCCEEDED;
    }

    bool preInterceptEvent(Ichor::TimerEvent const * const evt) {
        return AllowOthersHandling; //PreventOthersHandling if this event should be discarded
    }

    void postInterceptEvent(Ichor::TimerEvent const * const evt, bool processed) {
        // Can use this to track how long the processing took
    }

    Ichor::EventInterceptorRegistration _interceptor{};
};

int communication() {
    Ichor::CommunicationChannel channel{};
    DependencyManager dmOne{std::make_unique<MultimapQueue>()}; // ID = 0
    DependencyManager dmTwo{std::make_unique<MultimapQueue>()}; // ID = 1

    // Register the manager to the channel
    channel.addManager(&dmOne);
    channel.addManager(&dmTwo);

    std::thread t1([&dmOne] {
        dmOne.createServiceManager<Ichor::CoutFrameworkLogger, Ichor::IFrameworkLogger>();
        // your services here
        dmOne.start();
    });

    std::thread t2([&dmTwo] {
        dmTwo.createServiceManager<Ichor::CoutFrameworkLogger, Ichor::IFrameworkLogger>();
        // your services here
        dmTwo.start();
    });

    t1.join();
    t2.join();

    return 0;
}

struct MyMemoryStructure {
    MyMemoryStructure(int id_) : id(id_) {}
    uint64_t id;
    // etc
};

struct MyMemoryAllocatorService final : public Ichor::Service<MyMemoryAllocatorService> {
    StartBehaviour start() final {
        _myDataStructure = std::make_unique<MyMemoryStructure>(1);
        return StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        _myDataStructure.reset();
        return StartBehaviour::SUCCEEDED;
    }

    std::unique_ptr<MyMemoryStructure> _myDataStructure{};
};