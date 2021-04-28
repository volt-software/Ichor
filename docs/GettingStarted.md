# Getting Started

## Compiling Ichor

Compiling is done through the help of CMake. Ichor requires at least gcc 10.2 and has also been tested with gcc 10.3. Clang does not yet implement the required C++20 bits and I do not have access to a windows machine currently.

### Dependencies

### Ubuntu 20.04:

```
sudo add-apt-repository ppa:ubuntu-toolchain-r/ppa
sudo apt update
sudo apt install g++-10 build-essential cmake
```

#### Optional Features
Some features are behind feature flags and have their own dependencies.

If using etcd:
```
sudo apt install libgrpc++-dev libprotobuf-dev
```

If using the Boost.BEAST (recommended boost 1.70 or newer):
```
sudo apt install libboost1.71-all-dev libssl-dev
```

### CMake Options

#### BUILD_EXAMPLES

Builds the examples in the [examples directory](../examples). Takes some time to compile.

#### BUILD_BENCHMARKS

Builds the benchmarks in the [benchmarks directory](../benchmarks).

#### BUILD_TESTS

Builds the tests in the [test directory](../test).

#### USE_SPDLOG (optional dependency)

Enables the use of the [spdlog submodule](../external/spdlog). If examples or benchmarks are enabled, these then use the spdlog variants of loggers instead of cout.

#### USE_RAPIDJSON (optional dependency)

Enables the use of the [rapidjson submodule](../external/spdlog). Used for the serializer examples and benchmarks.

#### USE_PUBSUB

Not implemented currently.

#### USE_ETCD (optional dependency)

Enables the use of the [rapidjson submodule](../external/spdlog). Used for the etcd examples and benchmarks.

#### USE_BOOST_BEAST

Requires Boost.BEAST to be installed as a system dependency (version >= 1.70). Used for websocket and http server/client implementations.

#### USE_SANITIZERS

Compiles everything (including the optionally enabled submodules) with the AddressSanitizer and UndefinedBehaviourSanitizer. Recommended when debugging. Cannot be combined with the ThreadSanitizer

#### USE_THREAD_SANITIZER

Compiles everything (including the optionally enabled submodules) with the ThreadSanitizer. Recommended when debugging. Cannot be combined with the AddressSanitizer.

#### USE_UGLY_HACK_EXCEPTION_CATCHING

Debugging Boost.asio and Boost.BEAST is difficult, this hack enables catching exceptions directly at the source, rather than at the last point of being rethrown. Requires gcc.
#### REMOVE_SOURCE_NAMES

Ichor's logging macros by default adds the current filename and line number to each log statement. This option disables that.

## Your first Ichor program

### Starting the main thread

Starting a dependency manager is quite easy. Instantiate it, tell it which services to register and start it.

```c++
// main.cpp
#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/CoutFrameworkLogger.h>

int main() {
    Ichor::DependencyManager dm{};
    dm.createServiceManager<Ichor::CoutFrameworkLogger, Ichor::IFrameworkLogger>(); // Without a framework logger, the manager won't start!
    dm.start();
    
    return 0;
}
```

### Registering your first service

Just starting a manager without any custom services is not very interesting. So let's start populating it!

```c++
// MyService.h
#include <ichor/Service.h>

struct IMyService {}; // the interface

struct MyService final : public IMyService, public Ichor::Service<MyService> {}; // a minimal implementation
```

```c++
// main.cpp
#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/CoutFrameworkLogger.h>
#include "MyService.h" // Add this

int main() {
    Ichor::DependencyManager dm{};
    dm.createServiceManager<Ichor::CoutFrameworkLogger, Ichor::IFrameworkLogger>();
    dm.createServiceManager<MyService, IMyService>(); // Add this
    dm.start();
    
    return 0;
}
```



### Registering a dependency to your service

Now that we have a basic service registered and instantiated, let's add a service which depends on it:

```c++
// MyDependencyService.h
#include <ichor/Service.h>
#include <iostream>

struct IMyDependencyService {};

struct MyDependencyService final : public IMyDependencyService, public Ichor::Service<MyDependencyService> {
    MyDependencyService(Ichor::DependencyRegister &reg, Ichor::Properties props, Ichor::DependencyManager *mng) : Ichor::Service(std::move(props), mng) {
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
```

The service requires this specific constructor, which means that any extra arguments are not possible. The constructor provides us with three things:
* A variable with which we can register dependencies
* The properties with which this instance has been created
* The manager this service is registered to

```c++
// main.cpp
#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/CoutFrameworkLogger.h>
#include "MyService.h"
#include "MyDependencyService.h" // Add this

int main() {
    Ichor::DependencyManager dm{};
    dm.createServiceManager<Ichor::CoutFrameworkLogger, Ichor::IFrameworkLogger>();
    dm.createServiceManager<MyService, IMyService>();
    dm.createServiceManager<MyDependencyService, IMyDependencyService>(); // Add this
    dm.start();
    
    return 0;
}
```

And we complete it by registering it to the manager.

### Using a timer by registering an event handler

Before we get to the point where we are able to start and stop the program, let's add one more service:

```c++
// MyTimerService.h
#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/timer_bundle/TimerService.h>

struct IMyTimerService {};

struct MyTimerService final : public IMyTimerService, public Ichor::Service<MyTimerService> {
    bool start() {
        auto timer = getManager()->createServiceManager<Ichor::Timer, Ichor::ITimer>();
        timer->setChronoInterval(std::chrono::seconds(1));
        _timerEventRegistration = getManager()->registerEventHandler<Ichor::TimerEvent>(this, timer->getServiceId());
        timer->startTimer();
        return true;
    }
    
    bool stop() {
        _timerEventRegistration.reset();
        return true;
    }

    Ichor::Generator<bool> handleEvent(Ichor::TimerEvent const * const) {
        co_return (bool)PreventOthersHandling;        
    }

    std::unique_ptr<Ichor::EventHandlerRegistration, Ichor::Deleter> _timerEventRegistration{nullptr};
};
```

This service does a couple things:
* It overrides the start and stop methods, which are virtual methods from the `Service` parent.
If they are present, they are called when all required dependencies are met. In this case, since there are no dependencies, the service is started as soon as possible.
* In the start function, it starts another service: a timer. The `main.cpp` file is not the only place this is allowed in! The created service is not started yet, but it is possible to start using its interface straight away. In this example, we set the timer to trigger every second.
* Register an event handler for all `TimerEvent` types. The second parameter ensures that we only register it for events created by the timer service that we ourselves just created.

Registering an event handler requires adding the `handleEvent` function with this exact signature. The TimerEvent contains information such as which service created it, but for this example we don't need the event.
The return type a c++20 coroutine. Currently only `co_return` and `co_yield` are supported, both which require a boolean. The options are `PreventOthersHandling` and `AllowOthersHandling`, which tell Ichor whether other registered event handlers for this type are allowed to handle this event.
Since we created the timer, no one else has any use for it and therefore we disallow others.

```c++
// main.cpp
#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/CoutFrameworkLogger.h>
#include "MyService.h"
#include "MyDependencyService.h"
#include "MyTimerService.h" // Add this

int main() {
    Ichor::DependencyManager dm{};
    dm.createServiceManager<Ichor::CoutFrameworkLogger, Ichor::IFrameworkLogger>();
    dm.createServiceManager<MyService, IMyService>();
    dm.createServiceManager<MyDependencyService, IMyDependencyService>();
    dm.createServiceManager<MyTimerService, IMyTimerService>(); // Add this
    dm.start();
    
    return 0;
}
```

We also add this to the main file.

### Quitting the program

The only thing left to do is tell Ichor to stop. This can easily be done by pushing a `QuitEvent`, like so:

```c++
// MyTimerService.h
#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/timer_bundle/TimerService.h>

struct IMyTimerService {};

struct MyTimerService final : public IMyTimerService, public Ichor::Service<MyTimerService> {
    bool start() {
        auto timer = getManager()->createServiceManager<Ichor::Timer, Ichor::ITimer>();
        timer->setChronoInterval(std::chrono::seconds(1));
        _timerEventRegistration = getManager()->registerEventHandler<Ichor::TimerEvent>(this, timer->getServiceId());
        timer->startTimer();
        return true;
    }
    
    bool stop() {
        _timerEventRegistration.reset();
        return true;
    }

    Ichor::Generator<bool> handleEvent(Ichor::TimerEvent const * const) {
        getManager()->pushEvent<Ichor::QuitEvent>(getServiceId()); // Add this
        co_return (bool)PreventOthersHandling;
    }
    
    std::unique_ptr<Ichor::EventHandlerRegistration, Ichor::Deleter> _timerEventRegistration{nullptr};
};
```

And there you have it, your first working Ichor program!

## Advanced Features

There are a couple more advanced features that Ichor has:

### Services without interfaces

Registering a service is best done with an interface, as this allows other services to register dependencies for it. However, it is optional. The following service is allowed but cannot be used as a dependency:

```c++
// ServiceWithoutInterface.h
#include <ichor/Service.h>

struct ServiceWithoutInterface final : public Ichor::Service<ServiceWithoutInterface> {};
```

### Interceptors

It is possible to intercept all events before they're handled by registered services (or Ichor itself!). An example:

```c++
// MyInterceptorService.h
#include <ichor/DependencyManager.h>
#include <ichor/Events.h>

struct MyInterceptorService final : public Ichor::Service<MyInterceptorService> {
    bool start() final {
        _interceptor = this->getManager()->template registerEventInterceptor<Ichor::TimerEvent>(this); // Can change TimerEvent to just Event if you want to intercept *all* events
        return true;
    }
    
    bool stop() final {
        _interceptor.reset();
        return true;
    }
    
    bool preInterceptEvent(Ichor::TimerEvent const * const evt) {
        return AllowOthersHandling; //PreventOthersHandling if this event should be discarded
    }
    
    void postInterceptEvent(Ichor::TimerEvent const * const evt, bool processed) {
        // Can use this to track how long the processing took
    }

    std::unique_ptr<Ichor::EventInterceptorRegistration, Ichor::Deleter> _interceptor{};
};
```

### Priority

Ichor gives every event a default priority, but if necessary, you can push events with a higher priority so that they get processed before others. By default, the priority given to events is 1000, where lower numbers are higher priority. The lowest priority given is `std::numeric_limits<uint64_t>::max()`.

Pushing an event with a priority is done with the `pushPrioritisedEvent` function:
```c++
getManager()->pushPrioritisedEvent<TimerEvent>(getServiceId(), 10);
```

### Memory allocation

By default, Ichor uses `std::pmr::get_default_resource()` as the memory allocator for everything internal that allocates memory: events, service creation, shared/unique pointers, you name it.
Therefore, the easiest way to get some extra performance is to use `std::pmr::synchronized_pool_resource` as the default resource. However, the second constructor for the DependencyManager allows for some more fine-grained control.

The DependencyManager actually has two memory allocators: one which is guarded by mutexes for pushing/popping events into/from the queue and another for everything else like services.
The best performance can be gotten by instantiating a manager with two `std::pmr::unsynchronized_pool_resource` resources:

```c++
// main.cpp
#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/CoutFrameworkLogger.h>

int main() {
    std::pmr::unsynchronized_pool_resource resourceOne{};
    std::pmr::unsynchronized_pool_resource resourceTwo{};
    Ichor::DependencyManager dm{&resourceOne, &resourceTwo};
    dm.createServiceManager<Ichor::CoutFrameworkLogger, Ichor::IFrameworkLogger>();
    // your services here
    dm.start();
    
    return 0;
}
```

But as can be seen in the [realtime example](../examples/realtime_example/main.cpp), it is also possible to create allocators which only use stack-created memory, eliminating all dynamic memory allocations from Ichor. 

Using the memory allocator from your own Services is relatively easy:

```c++
// MyMemoryAllocatorService.h
#include <ichor/DependencyManager.h>
#include <ichor/Events.h>

struct MyMemoryStructure {
    uint64_t id;
    // etc
};

struct MyMemoryAllocatorService final : public Ichor::Service<MyMemoryAllocatorService> {
    bool start() final {
        _myDataStructure = std::unique_ptr<MyMemoryStructure, Ichor::Deleter>(new (getMemoryResource()->allocate(sizeof(MyMemoryStructure))) MyMemoryStructure(1), Ichor::Deleter{getMemoryResource(), sizeof(MyMemoryStructure)});
        return true;
    }
    
    bool stop() final {
        _myDataStructure.reset();
        return true;
    }

    std::unique_ptr<MyMemoryStructure, Ichor::Deleter> _myDataStructure{};
};
```

### Multiple Threads

All the effort into thread-safety would be for naught if it weren't possible to use Ichor with multiple threads.


Starting up two manager is easy, but allowing services to communicate to services in another thread requires some setup:

```c++
// main.cpp
#include <ichor/DependencyManager.h>
#include <ichor/CommunicationChannel.h>
#include <ichor/optional_bundles/logging_bundle/CoutFrameworkLogger.h>

int main() {
    Ichor::CommunicationChannel channel{};
    DependencyManager dmOne{}; // ID = 0
    DependencyManager dmTwo{}; // ID = 1
    
    // Register the manager to the channel
    channel.addManager(&dmOne);
    channel.addManager(&dmTwo);
    
    std::thread t1([&dmOne] {
        dmOne.createServiceManager<Ichor::CoutFrameworkLogger, Ichor::IFrameworkLogger>();
        // your services here
        dmOne.start();
    };

    std::thread t2([&dmTwo] {
        dmTwo.createServiceManager<Ichor::CoutFrameworkLogger, Ichor::IFrameworkLogger>();
        // your services here
        dmTwo.start();
    };
    
    t1.join();
    t2.join();
    
    return 0;
}
```

This then allows services to send events to other manager:

```c++
// MyCommunicatingService.h
#include <ichor/DependencyManager.h>
#include <ichor/Events.h>

struct MyMemoryStructure {
    uint64_t id;
    // etc
};

struct MyCommunicatingService final : public Ichor::Service<MyCommunicatingService> {
    bool start() final {
        getManager()->getCommunicationChannel()->broadcastEvent<QuitEvent>(getManager(), getServiceId()); // sends to all other managers, except the one this service is registered to
        return true;
    }
};
```

The communication channel also has a `sendEventTo` function, which allows sending to a specific manager. Manager IDs are deterministic, the ID starts at 0 and increments by one for every created manager. See the comments above for `main.cpp` for an example.  