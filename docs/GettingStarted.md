# Getting Started

## Compiling Ichor

Compiling is done through the help of CMake. Ichor requires at least gcc 11.2 (11.3 recommended due to [this gcc bug](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=95137)) or clang 14, and is tested with gcc 11.3, 12, clang 14, clang 15 and clang 16.
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

#### ICHOR_USE_SPDLOG (optional dependency)

Enables the use of the [spdlog submodule](../external/spdlog). If examples or benchmarks are enabled, these then use the spdlog variants of loggers instead of cout.

#### USE_RAPIDJSON (optional dependency)

Enables the use of the [rapidjson submodule](../external/spdlog). Used for the serializer examples and benchmarks.

#### ICHOR_USE_PUBSUB

Not implemented currently.

#### ICHOR_USE_ETCD (optional dependency)

Enables the use of the [rapidjson submodule](../external/spdlog). Used for the etcd examples and benchmarks.

#### ICHOR_USE_BOOST_BEAST

Requires Boost.BEAST to be installed as a system dependency (version >= 1.70). Used for websocket and http server/client implementations.

#### ICHOR_USE_SANITIZERS

Compiles everything (including the optionally enabled submodules) with the AddressSanitizer and UndefinedBehaviourSanitizer. Recommended when debugging. Cannot be combined with the ThreadSanitizer

#### ICHOR_USE_THREAD_SANITIZER

Compiles everything (including the optionally enabled submodules) with the ThreadSanitizer. Recommended when debugging. Cannot be combined with the AddressSanitizer.

#### ICHOR_USE_UGLY_HACK_EXCEPTION_CATCHING

Debugging Boost.asio and Boost.BEAST is difficult, this hack enables catching exceptions directly at the source, rather than at the last point of being rethrown. Requires gcc.
#### ICHOR_REMOVE_SOURCE_NAMES

Ichor's logging macros by default adds the current filename and line number to each log statement. This option disables that.

## Your first Ichor program

### Starting the main thread

Starting a dependency manager is quite easy. Instantiate it, tell it which services to register and start it.

```c++
// main.cpp
#include <ichor/DependencyManager.h>
#include <ichor/services/logging/CoutFrameworkLogger.h>

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

struct MyService final : public IMyService, public Ichor::Service<MyService> {
    MyService() = default;
}; // a minimal implementation
```

```c++
// main.cpp
#include <ichor/DependencyManager.h>
#include <ichor/services/logging/CoutFrameworkLogger.h>
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
```

The service requires this specific constructor, which means that any extra arguments are not possible. The constructor provides us with three things:
* A variable with which we can register dependencies
* The properties with which this instance has been created
* The manager this service is registered to

```c++
// main.cpp
#include <ichor/DependencyManager.h>
#include <ichor/services/logging/CoutFrameworkLogger.h>
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
#include <ichor/services/timer/TimerService.h>

struct IMyTimerService {};

struct MyTimerService final : public IMyTimerService, public Ichor::Service<MyTimerService> {
    MyTimerService() = default;

    Ichor::StartBehaviour start() final {
        auto timer = getManager().createServiceManager<Ichor::Timer, Ichor::ITimer>();
        timer->setChronoInterval(std::chrono::seconds(1));
        _timerEventRegistration = getManager().registerEventHandler<Ichor::TimerEvent>(this, timer->getServiceId());
        timer->startTimer();
        return Ichor::StartBehaviour::SUCCEEDED;
    }
    
    Ichor::StartBehaviour stop() final {
        _timerEventRegistration.reset();
        return Ichor::StartBehaviour::SUCCEEDED;
    }

    Ichor::Generator<bool> handleEvent(Ichor::TimerEvent const * const) {
        co_return;        
    }

    Ichor::EventHandlerRegistration _timerEventRegistration{};
};
```

This service does a couple things:
* It overrides the start and stop methods, which are virtual methods from the `Service` parent.
If they are present, they are called when all required dependencies are met. In this case, since there are no dependencies, the service is started as soon as possible.
* In the start function, it starts another service: a timer. The `main.cpp` file is not the only place this is allowed in! The created service is not started yet, but it is possible to start using its interface straight away. In this example, we set the timer to trigger every second.
* Register an event handler for all `TimerEvent` types. The second parameter ensures that we only register it for events created by the timer service that we ourselves just created.

Registering an event handler requires adding the `handleEvent` function with this exact signature. The TimerEvent contains information such as which service created it, but for this example we don't need the event.
The return type a c++20 coroutine. Currently `co_return`, `co_await` and `co_yield` are supported on `AsyncGenerator<T>`.

```c++
// main.cpp
#include <ichor/DependencyManager.h>
#include <ichor/services/logging/CoutFrameworkLogger.h>
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
#include <ichor/services/timer/TimerService.h>

struct IMyTimerService {};

struct MyTimerService final : public IMyTimerService, public Ichor::Service<MyTimerService> {
    MyTimerService() = default;

    Ichor::StartBehaviour start() final {
        auto timer = getManager().createServiceManager<Ichor::Timer, Ichor::ITimer>();
        timer->setChronoInterval(std::chrono::seconds(1));
        _timerEventRegistration = getManager().registerEventHandler<Ichor::TimerEvent>(this, timer->getServiceId());
        timer->startTimer();
        return Ichor::StartBehaviour::SUCCEEDED;
    }
    
    Ichor::StartBehaviour stop() final {
        _timerEventRegistration.reset();
        return Ichor::StartBehaviour::SUCCEEDED;
    }

    Ichor::Generator<bool> handleEvent(Ichor::TimerEvent const * const) {
        getManager().pushEvent<Ichor::QuitEvent>(getServiceId()); // Add this
        co_return;
    }
    
    Ichor::EventHandlerRegistration _timerEventRegistration{};
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

struct ServiceWithoutInterface final : public Ichor::Service<ServiceWithoutInterface> {
    ServiceWithoutInterface() = default;
};
```

### Interceptors

It is possible to intercept all events before they're handled by registered services (or Ichor itself!). An example:

```c++
// MyInterceptorService.h
#include <ichor/DependencyManager.h>
#include <ichor/Events.h>
#include <ichor/services/timer/TimerService.h>

struct MyInterceptorService final : public Ichor::Service<MyInterceptorService> {
    Ichor::StartBehaviour start() final {
        _interceptor = this->getManager().template registerEventInterceptor<Ichor::RunFunctionEvent>(this); // Can change TimerEvent to just Event if you want to intercept *all* events
        return Ichor::StartBehaviour::SUCCEEDED;
    }
    
    Ichor::StartBehaviour stop() final {
        _interceptor.reset();
        return Ichor::StartBehaviour::SUCCEEDED;
    }
    
    bool preInterceptEvent(Ichor::RunFunctionEvent const &) {
        return AllowOthersHandling; //PreventOthersHandling if this event should be discarded
    }
    
    void postInterceptEvent(Ichor::RunFunctionEvent const &, bool processed) {
        // Can use this to track how long the processing took
    }

    std::unique_ptr<Ichor::EventInterceptorRegistration> _interceptor{};
};
```

### Priority

Ichor gives every event a default priority, but if necessary, you can push events with a higher priority so that they get processed before others. By default, the priority given to events is 1000, where lower numbers are higher priority. The lowest priority given is `std::numeric_limits<uint64_t>::max()`.

Pushing an event with a priority is done with the `pushPrioritisedEvent` function:
```c++
getManager().pushPrioritisedEvent<TimerEvent>(getServiceId(), 10);
```

### Memory allocation

Ichor used to provide `std::pmr::memory_resource` based allocation, however that had a big impact on the ergonomy of the code. Moreover, clang 14 does not support `<memory_resource>` at all. Instead, Ichor recommends using tcmalloc or jemalloc to reduce the resource contention when using multiple threads.

### Multiple Threads

All the effort into thread-safety would be for naught if it weren't possible to use Ichor with multiple threads.


Starting up two manager is easy, but allowing services to communicate to services in another thread requires some setup:

```c++
// main.cpp
#include <ichor/DependencyManager.h>
#include <ichor/CommunicationChannel.h>
#include <ichor/services/logging/CoutFrameworkLogger.h>

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
```

This then allows services to send events to other manager:

```c++
// MyCommunicatingService.h
#include <ichor/DependencyManager.h>
#include <ichor/Events.h>

struct MyCommunicatingService final : public Ichor::Service<MyCommunicatingService> {
    Ichor::StartBehaviour start() final {
        getManager().getCommunicationChannel()->broadcastEvent<QuitEvent>(getManager(), getServiceId()); // sends to all other managers, except the one this service is registered to
        return Ichor::StartBehaviour::SUCCEEDED;
    }
};
```

The communication channel also has a `sendEventTo` function, which allows sending to a specific manager. Manager IDs are deterministic, the ID starts at 0 and increments by one for every created manager. See the comments above for `main.cpp` for an example.  