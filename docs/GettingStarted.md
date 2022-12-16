# Getting Started

## Compiling Ichor

Compiling is done through the help of CMake. Ichor requires at least gcc 11.3 (due to [this gcc bug](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=95137)) or clang 14, and is tested with gcc 11.3, 12.1, clang 14, clang 15 and clang 16.

### Dependencies

#### Ubuntu 20.04:

```
sudo apt install libisl-dev libmpfrc++-dev libmpc-dev libgmp-dev build-essential cmake g++
wget http://mirror.koddos.net/gcc/releases/gcc-11.3.0/gcc-11.3.0.tar.xz
tar xf gcc-11.3.0.tar.xz
mkdir gcc-build
cd gcc-build
../gcc-11.3.0/configure --prefix=/opt/gcc-11.3 --enable-languages=c,c++ --disable-multilib
make -j$(nproc)
sudo make install
```

Then with cmake, use
```
CXX=/opt/gcc-11.3.0/bin/g++ cmake $PATH_TO_ICHOR_SOURCE
```

#### Ubuntu 22.04:

```
sudo apt install g++ build-essential cmake
```

#### Optional Features
Some features are behind feature flags and have their own dependencies.

If using etcd:
```
sudo apt install libgrpc++-dev libprotobuf-dev
```

If using the Boost.BEAST (recommended boost 1.70 or newer):

Ubuntu 20.04:
```
sudo apt install libboost1.71-all-dev libssl-dev
```

Ubuntu 22.04:
```
sudo apt install libboost1.74-all-dev libssl-dev
```

If using abseil:
Ubuntu 20.04:
Not available on apt. Please compile manually.

Ubuntu 22.04:
```
sudo apt install libabsl-dev
```

#### Windows

Install MSVC 17.4 or newer. Open Ichor in MSVC and configure CMake according to your wishes. Build and install and you should find an `out` directory in Ichor's top level directory.

Use that directory in your personal project, preferably with cmake as Ichor exports compile-time definitions in it.

If boost is desired, please download the [windows prebuilt packages](https://sourceforge.net/projects/boost/files/boost-binaries/) (`boost_1_80_0-msvc-14.3-64.exe` is the latest at time of writing).

Then add the following [system variables](https://www.alphr.com/set-environment-variables-windows-11/), with the path you've extracted boost into:
```
    BOOST_INCLUDEDIR    C:\SDKs\boost_1_80_0\
    BOOST_LIBRARYDIR    C:\SDKs\boost_1_80_0\lib64-msvc-14.3
    BOOST_ROOT          C:\SDKs\boost_1_80_0\boost
```

To run the examples/tests that use boost, copy the dlls in `C:\SDKs\boost_1_80_0\lib64-msvc-14.3` (or where you installed boost) into the generated `bin` folder.

#### OSX Monterey

Work in progress, initial support available, sanitizers with boost seem to get false positives.

```shell
brew install llvm@15
brew install ninja
brew install boost
brew install cmake
```

Then with cmake, set the CC and CXX variables explicitly:
```shell
CC=$(brew --prefix llvm)/bin/clang CXX=$(brew --prefix llvm)/bin/clang++ cmake -GNinja -DICHOR_USE_SANITIZERS=OFF -DICHOR_SERIALIZATION_FRAMEWORK=RAPIDJSON -DICHOR_USE_BOOST_BEAST=ON ..
ninja
```

#### CMakeLists.txt

To use Ichor, compile and install it in a location that cmake can find (e.g. /usr) and use the following CMakeLists.txt:

```cmake
cmake_minimum_required(VERSION 3.12)
project(my_project)

set(CMAKE_CXX_STANDARD 20)
find_package(Ichor CONFIG REQUIRED)

add_executable(my_exe main.cpp)
target_link_libraries(my_exe Ichor::ichor)
```

### CMake Options

#### BUILD_EXAMPLES

Builds the examples in the [examples directory](../examples). Takes some time to compile.

#### BUILD_BENCHMARKS

Builds the benchmarks in the [benchmarks directory](../benchmarks).

#### BUILD_TESTS

Builds the tests in the [test directory](../test).

#### ICHOR_ENABLE_INTERNAL_DEBUGGING

Enables verbose logging at various points in the Ichor framework. Recommended only when trying to figure out if you've encountered a bug in Ichor.

#### ICHOR_USE_SPDLOG (optional dependency)

Enables the use of the [spdlog submodule](../external/spdlog). If examples or benchmarks are enabled, these then use the spdlog variants of loggers instead of cout.

#### ICHOR_USE_PUBSUB

Not implemented currently.

#### ICHOR_USE_ETCD (optional dependency)
 
Used for the etcd examples and benchmarks.

#### ICHOR_USE_BOOST_BEAST

Requires Boost.BEAST to be installed as a system dependency (version >= 1.70). Used for websocket and http server/client implementations. All examples require `ICHOR_SERIALIZATION_FRAMEWORK` to be set as well.

#### ICHOR_SERIALIZATION_FRAMEWORK

Controls which JSON serializer to be used in benchmarks and examples.

Can be one of: OFF RAPIDJSON BOOST_JSON

BOOST_JSON requires boost to be installed (version >= 1.75).

#### ICHOR_USE_SANITIZERS

Compiles everything (including the optionally enabled submodules) with the AddressSanitizer and UndefinedBehaviourSanitizer. Recommended when debugging. Cannot be combined with the ThreadSanitizer

#### ICHOR_USE_THREAD_SANITIZER

Compiles everything (including the optionally enabled submodules) with the ThreadSanitizer. Recommended when debugging. Cannot be combined with the AddressSanitizer. Note that compiler bugs may exist, like for [gcc](https://gcc.gnu.org/bugzilla//show_bug.cgi?id=101978). 

#### ICHOR_USE_UGLY_HACK_EXCEPTION_CATCHING

Debugging Boost.Asio and Boost.BEAST is difficult, this hack enables catching exceptions directly at the source, rather than at the last point of being rethrown. Requires gcc.

#### ICHOR_REMOVE_SOURCE_NAMES

Ichor's logging macros by default adds the current filename and line number to each log statement. This option disables that.

#### ICHOR_USE_MOLD

For clang compilers, add the `-fuse-ld=mold` linker flag. This speeds up the linking stage.
Usage with gcc 12+ is technically possible, but throws off [Catch2 unit test detection](https://github.com/catchorg/Catch2/issues/2507).

#### ICHOR_USE_SDEVENT

Enables the use of the [sdevent event queue](../include/ichor/event_queues/SdeventQueue.h). Requires having sdevent headers and libraries installed on your system.

#### ICHOR_USE_ABSEIL

Enables the use of the abseil containers in Ichor. Requires having abseil headers and libraries installed on your system.

#### ICHOR_DISABLE_RTTI

Disables `dynamic_cast<>()` in most cases as well as `typeid`. Ichor is an opinionated piece of software and we strongly encourage you to disable RTTI. We believe `dynamic_cast<>()` is wrong in almost all instances. Virtual methods and double dispatch should be used instead. If, however, you really want to use RTTI, use this option to re-enable it. 

#### ICHOR_USE_MIMALLOC

If `ICHOR_USE_SANITIZERS` is turned OFF, Ichor by default compiles itself with mimalloc, speeding up the runtime a lot and reducing peak memory usage.

#### ICHOR_USE_SYSTEM_MIMALLOC

If `ICHOR_USE_MIMALLOC` is turned ON, this option can be used to use the system installed mimalloc instead of the Ichor provided one.

## Your first Ichor program

### Starting the main thread

Starting a dependency manager is quite easy. Instantiate it, tell it which services to register and start it.

```c++
// main.cpp
#include <ichor/DependencyManager.h>
#include <ichor/event_queues/MultimapQueue.h>

int main() {
    auto queue = std::make_unique<MultimapQueue>(); // use a priority queue based on Multimap
    auto &dm = queue->createManager(); // create the dependency manager
    dm.start();
    
    return 0;
}
```

### Registering your first service

Just starting a manager without any custom services is not very interesting. So let's start populating it!

```c++
// MyService.h
#include <ichor/DependencyManager.h>
#include <ichor/Service.h>

struct IMyService {}; // the interface

struct MyService final : public IMyService, public Ichor::Service<MyService> {
    MyService() = default;
}; // a minimal implementation
```

```c++
// main.cpp
#include <ichor/DependencyManager.h>
#include <ichor/event_queues/MultimapQueue.h>
#include "MyService.h" // Add this

int main() {
    auto queue = std::make_unique<MultimapQueue>();
    auto &dm = queue->createManager();
    dm.createServiceManager<MyService, IMyService>(); // Add this
    dm.start();
    
    return 0;
}
```



### Registering a dependency to your service

Now that we have a basic service registered and instantiated, let's add a service which depends on it:

```c++
// MyDependencyService.h
#include <ichor/DependencyManager.h>
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
#include <ichor/event_queues/MultimapQueue.h>
#include "MyService.h"
#include "MyDependencyService.h" // Add this

int main() {
    auto queue = std::make_unique<MultimapQueue>();
    auto &dm = queue->createManager();
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

    Ichor::AsyncGenerator<void> start() final {
        auto timer = getManager().createServiceManager<Ichor::Timer, Ichor::ITimer>();
        timer->setChronoInterval(std::chrono::seconds(1));
        _timerEventRegistration = getManager().registerEventHandler<Ichor::TimerEvent>(this, timer->getServiceId());
        timer->startTimer();
        co_return;
    }
    
    Ichor::AsyncGenerator<void> stop() final {
        _timerEventRegistration.reset();
        co_return;
    }

    Ichor::AsyncGenerator<IchorBehaviour> handleEvent(Ichor::TimerEvent const &) {
        fmt::print("Timer callback\n");
        co_return {};        
    }

    Ichor::EventHandlerRegistration _timerEventRegistration{};
};
```

This service does a couple of things:
* It overrides the start and stop methods, which are virtual methods from the `Service` parent.
If they are present, they are called when all required dependencies are met. In this case, since there are no dependencies, the service is started as soon as possible.
* In the start function, it starts another service: a timer. The `main.cpp` file is not the only place this is allowed in! The created service is not started yet, but it is possible to start using its interface straight away. In this example, we set the timer to trigger every second.
* Register an event handler for all `TimerEvent` types. The second parameter ensures that we only register it for events created by the timer service that we ourselves just created.

Registering an event handler requires adding the `handleEvent` function with this exact signature. The TimerEvent contains information such as which service created it, but for this example we don't need the event.
The return type a c++20 coroutine. Currently `co_return`, `co_await` and `co_yield` are supported on `AsyncGenerator<T>`.

```c++
// main.cpp
#include <ichor/DependencyManager.h>
#include <ichor/event_queues/MultimapQueue.h>
#include "MyService.h"
#include "MyDependencyService.h"
#include "MyTimerService.h" // Add this

int main() {
    auto queue = std::make_unique<MultimapQueue>();
    auto &dm = queue->createManager();
    dm.createServiceManager<MyService, IMyService>();
    dm.createServiceManager<MyDependencyService, IMyDependencyService>();
    dm.createServiceManager<MyTimerService, IMyTimerService>(); // Add this
    dm.start();
    
    return 0;
}
```

We also add this to the main file.

Using event handlers for timers is a lot of boilerplate, but this example allows us to demonstrate multiple concepts. Normally, one would use timers like so:

```c++
    Ichor::AsyncGenerator<void> start() final {
        _timer = getManager().createServiceManager<Ichor::Timer, Ichor::ITimer>();
        _timer->setChronoInterval(std::chrono::seconds(1));
        // Synchronous callback
        _timer->setCallback(this, []() {
            fmt::print("Timer callback\n");
        });
        // Asynchronous callback, choose one of the two, not both
        _timer->setCallbackAsync(this, []() -> AsyncGenerator<IchorBehaviour> {
            fmt::print("Timer callback\n");
            // maybe a co_await here
            co_return {};
        });
        _timer->startTimer();
        co_return;
    }
    
    Timer *_timer{};
```

### Quitting the program

The only thing left to do is tell Ichor to stop. This can easily be done by pushing a `QuitEvent`, like so:

```c++
// MyTimerService.h
#include <ichor/DependencyManager.h>
#include <ichor/services/timer/TimerService.h>

struct IMyTimerService {};

struct MyTimerService final : public IMyTimerService, public Ichor::Service<MyTimerService> {
    MyTimerService() = default;

    Ichor::AsyncGenerator<void> start() final {
        auto timer = getManager().createServiceManager<Ichor::Timer, Ichor::ITimer>();
        timer->setChronoInterval(std::chrono::seconds(1));
        _timerEventRegistration = getManager().registerEventHandler<Ichor::TimerEvent>(this, timer->getServiceId());
        timer->startTimer();
        co_return;
    }
    
    Ichor::AsyncGenerator<void> stop() final {
        _timerEventRegistration.reset();
        co_return;
    }

    Ichor::AsyncGenerator<IchorBehaviour> handleEvent(Ichor::TimerEvent const &) {
        getManager().pushEvent<Ichor::QuitEvent>(getServiceId()); // Add this
        co_return {};
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
    Ichor::AsyncGenerator<void> start() final {
        _interceptor = this->getManager().template registerEventInterceptor<Ichor::RunFunctionEvent>(this); // Can change TimerEvent to just Event if you want to intercept *all* events
        co_return {};
    }
    
    Ichor::AsyncGenerator<void> stop() final {
        _interceptor.reset();
        co_return {};
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
getManager().pushPrioritisedEvent<TimerEvent>(getServiceId(), 10u);
```

The default priority for events is 1000. For dependency related things (like start service, dependency online events) it is 100.

### Memory allocation

Ichor used to provide `std::pmr::memory_resource` based allocation, however that had a big impact on the ergonomy of the code. Moreover, clang 14 does not support `<memory_resource>` at all. Instead, Ichor recommends using mimalloc to reduce the resource contention when using multiple threads.

### Multiple Threads

All the effort into thread-safety would be for naught if it weren't possible to use Ichor with multiple threads.


Starting up two manager is easy, but allowing services to communicate to services in another thread requires some setup:

```c++
// main.cpp
#include <ichor/DependencyManager.h>
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/CommunicationChannel.h>

int main() {
    Ichor::CommunicationChannel channel{};
    auto queueOne = std::make_unique<MultimapQueue>();
    auto &dmOne = queue->createManager(); // ID = 0
    auto queueTwo = std::make_unique<MultimapQueue>();
    auto &dmTwo = queue->createManager(); // ID = 1
    
    // Register the manager to the channel
    channel.addManager(&dmOne);
    channel.addManager(&dmTwo);
    
    std::thread t1([&dmOne] {
        // your services here
        dmOne.start();
    });

    std::thread t2([&dmTwo] {
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
    Ichor::AsyncGenerator<void> start() final {
        getManager().getCommunicationChannel()->broadcastEvent<QuitEvent>(getManager(), getServiceId()); // sends to all other managers, except the one this service is registered to
        co_return {};
    }
};
```

The communication channel also has a `sendEventTo` function, which allows sending to a specific manager. Manager IDs are deterministic, the ID starts at 0 and increments by one for every created manager. See the comments above for `main.cpp` for an example.  
