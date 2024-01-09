# Getting Started

## Compiling Ichor

Compiling is done through the help of CMake. Ichor requires at least gcc 11.3 (due to [this gcc bug](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=95137)), clang 14 or MSVC 2022, and is tested with gcc 11.3, 12.1, clang 14, clang 15 and clang 16 and MSVC 2022.

The easiest is to build it with the provided Dockerfile:

```sh
docker build -f Dockerfile -t ichor . && docker run -v $(pwd):/opt/ichor/src -it ichor # for a release build
docker build -f Dockerfile-musl -t ichor-musl . && docker run -v $(pwd):/opt/ichor/src -it ichor-musl # for a release + musl build
docker build -f Dockerfile-asan -t ichor-asan . && docker run -v $(pwd):/opt/ichor/src -it ichor-asan # for a debug build with sanitizers
```

The binaries will then be in `$(pwd)/bin`. The musl build statically compiles Ichor and should be able to run on all platforms, otherwise the binaries need at least glibc 2.35 on the system that's running them.  

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

If using the Boost.BEAST (recommended boost 1.70 or newer):

Ubuntu 20.04:
```
sudo apt install libboost1.71-all-dev libssl-dev
```

Ubuntu 22.04:
```
sudo apt install libboost1.74-all-dev libssl-dev
```

#### Windows

Install MSVC 17.4 or newer. Open Ichor in MSVC and configure CMake according to your wishes. Build and install and you should find an `out` directory in Ichor's top level directory.

Use that directory in your personal project, preferably with cmake as Ichor exports compile-time definitions in it.

If boost is desired, please download the [windows prebuilt packages](https://sourceforge.net/projects/boost/files/boost-binaries/) (`boost_1_81_0-msvc-14.3-64.exe` is the latest at time of writing).

Then add the following [system variables](https://www.alphr.com/set-environment-variables-windows-11/), with the path you've extracted boost into:
```
    BOOST_INCLUDEDIR    C:\SDKs\boost_1_81_0\
    BOOST_LIBRARYDIR    C:\SDKs\boost_1_81_0\lib64-msvc-14.3
    BOOST_ROOT          C:\SDKs\boost_1_81_0\boost
```

To run the examples/tests that use boost, copy the dlls in `C:\SDKs\boost_1_81_0\lib64-msvc-14.3` (or where you installed boost) into the generated `bin` folder.

Something similar goes for openssl. Download the latest [prebuilt binaries here](https://github.com/CristiFati/Prebuilt-Binaries/tree/master/OpenSSL) and unpack it into `C:\SDKs\openssl_3.1.3` so that `C:\SDKs\openssl_3.1.3´\include` exists, skipping a few subdirectories. Then add the following environment variables:
```
    OPENSSL_INCLUDE_DIR   C:\SDKs\openssl_3.1.3\include
    OPENSSL_LIBRARYDIR    C:\SDKs\openssl_3.1.3\lib
    OPENSSL_ROOT          C:\SDKs\openssl_3.1.3
```

Don't forget to copy `C:\SDKs\openssl_3.1.3\bin\*.dll` to the Ichor `bin` directory after compiling Ichor.

#### OSX Monterey

Work in progress, initial support available, sanitizers with boost seem to get false positives.

```shell
brew install llvm
brew install ninja
brew install boost
brew install cmake
```

Then with cmake, set the CC and CXX variables explicitly:
```shell
CC=$(brew --prefix llvm)/bin/clang CXX=$(brew --prefix llvm)/bin/clang++ cmake -GNinja -DICHOR_USE_SANITIZERS=OFF -DICHOR_USE_BOOST_BEAST=ON ..
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
    dm.start(CaptureSigInt);
    
    return 0;
}
```

### Registering your first services

Just starting a manager without any custom services is not very interesting. So let's start populating it!

```c++
// SomeDependency.h
struct ISomeDependency {
    virtual void hello_world() = 0;
};
struct SomeDependency final : public ISomeDependency {
    SomeDependency() = default;
    void hello_world() final;
}; // a minimal implementation
```

```c++
// SomeDependency.cpp
#include <fmt/format.h>
#include "SomeDependency.h"
void SomeDependency::hello_world() {
    fmt::print("Hello, world!\n");
}
```

```c++
// MyService.h
#include "SomeDependency.h"
struct MyService final { // Don't need an interface here, nothing has a dependency on MyService
    MyService(ISomeDependency *dependency) {
        dependency->hello_world();
    }
}; // a minimal implementation
```

```c++
// main.cpp
#include <ichor/DependencyManager.h>
#include <ichor/event_queues/MultimapQueue.h>
#include "SomeDependency.h"
#include "MyService.h"

int main() {
    auto queue = std::make_unique<MultimapQueue>();
    auto &dm = queue->createManager();
    dm.createServiceManager<SomeDependency, ISomeDependency>(); // register SomeDependency as providing an ISomeDependency
    dm.createServiceManager<MyService>(); // register MyService (requested dependencies get registered automatically)
    dm.start(CaptureSigInt);
    
    return 0;
}
```

### Requesting Dependencies 

For more information on Dependency Injection, please see the [relevant doc](02-DependencyInjection.md).

In general, the arguments in a constructor are reflected upon on compile-time and are all considered to be requests. That means that there are no custom arguments possible. e.g.

```c++
struct CompileErrorService final {
    CompileErrorService(int i) { // `int` is not a struct/class nor is `i` a pointer.
    }
};
```

The following, however, is completely fine:

```c++
struct MyService final {
    MyService(IService1 *, IService2 *, IService3 *, IService4 * /* and so on */) {
    }
};
```

The limit is compiler dependent, but it is >100 as far as the author is aware on all compilers.

There are a couple of special dependency requests you can make in a service:

```c++
struct MyService final {
    MyService(IEventQueue *, DependencyManager *, IService *) {
    }
};
```

IEventQueue is always the underlying queue registered with the DependencyManager, providing a way to manipulate the event loop

DependencyManager is self-explanatory

IService is the underlying service created by the DependencyManager when the instance got created. This contains things like the service Id and service Properties.

### Manipulating the event loop

```c++
// EventManipulationService.h
#include <ichor/event_queues/IEventQueue.h>
#include <ichor/dependency_management/IService.h>
#include <fmt/format.h>
struct EventManipulationService final { // Don't need an interface here, nothing has a dependency on EventManipulationService
    MyService(IEventQueue *q, IService *self) {
        // push an event on the queue, to be executed later. 
        // Events use the service id of the originating service to identify where it came from.
        q->pushEvent<RunFunctionEvent>(self->getServiceId(), [&]() {
            fmt::print("Hello, world!\n");
            q->pushEvent<QuitEvent>(self->getServiceId()); // this event, when run, tells the Dependency Manager to stop, releasing the thread.
        });
    }
}; // a minimal implementation
```

```c++
// main.cpp
#include <ichor/DependencyManager.h>
#include <ichor/event_queues/MultimapQueue.h>
#include "EventManipulationService.h"

int main() {
    auto queue = std::make_unique<MultimapQueue>();
    auto &dm = queue->createManager();
    dm.createServiceManager<EventManipulationService>();
    dm.start(CaptureSigInt);
    
    fmt::print("This runs after the QuitEvent has successfully stopped the manager\n");
    
    return 0;
}
```

### Using a timer to execute a recurring event

Before we get to the point where we are able to start and stop the program, let's showcase one more service:

```c++
// MyTimerService.h
#include <ichor/services/timer/ITimerFactory.h>

struct MyTimerService final {
    MyTimerService(ITimerFactory *factory) {
        auto &timer = factory->createTimer();
        timer.setChronoInterval(std::chrono::seconds(1));
        timer.setCallback([this]() {
            fmt::print("Timer callback\n"); // this line gets executed once every second
        });
        timer.startTimer();
    }
};
```

```c++
// main.cpp
#include <ichor/DependencyManager.h>
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/services/timer/TimerFactoryFactory.h> // Add this
#include "MyService.h"
#include "MyDependencyService.h"
#include "MyTimerService.h" // Add this

int main() {
    auto queue = std::make_unique<MultimapQueue>();
    auto &dm = queue->createManager();
    dm.createServiceManager<MyService, IMyService>();
    dm.createServiceManager<MyDependencyService, IMyDependencyService>();
    dm.createServiceManager<MyTimerService>(); // Add this
    dm.createServiceManager<TimerFactoryFactory>(); // Add this
    dm.start();
    
    return 0;
}
```

The newly added `TimerFactoryFactory` listens for any services requesting a `ITimerFactory` and creates one on-the-fly. Timers impersonate the requesting service when inserting events into the queue and therefore need the underlying service id of the requesting service. The FactoryFactory seemlessly solves this without the requesting service ever knowing.
The flipside is that if the `TimerFactoryFactory` is not instantiated, the `MyTimerService` never starts, as its dependency never gets created.

### Coroutines

Now that we have a timer, we've got everything necessary to setup and use coroutines, a fancy new c++20 feature.
```c++
// AwaitService.h
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <ichor/coroutines/Task.h>
#include <ichor/event_queues/IEventQueue.h>
#include <thread>

using namespace std::chrono_literals;

struct IAwaitService {
    virtual Ichor::Task<int> WaitOneSecond() = 0;
};

struct AwaitService final : public IAwaitService {
    AwaitService(IEventQueue *queue) : _queue(queue) {
        
    }
    Ichor::Task<int> WaitOneSecond() final {
        Ichor::AsyncManualResetEvent evt{}; // storage for calling coroutine frame
        std::thread t([&]() {
            std::this_thread::sleep_for(1s);
            // If we want coroutines waiting on this function to resume on the same thread,
            // we have to call the `set` function from the queue, not our current thread.
            _queue->pushEvent<RunFunctionEvent>(0, [&]() {
                evt.set(); // resume waiting coroutines
            });
        });
        co_await evt; // pause execution until evt.set() is called
        co_return 5; // return 5 when we're done waiting
    }
    
    IEventQueue *_queue{};
};
```

```c++
// MyCoroutineTimerService.h
#include <ichor/services/timer/ITimerFactory.h>
#include "AwaitService.h"

struct MyCoroutineTimerService final {
    MyCoroutineTimerService(ITimerFactory *factory, IAwaitService *awaitService) {
        auto &timer = factory->createTimer();
        timer.setChronoInterval(std::chrono::seconds(1));
        timer.setCallbackAsync([awaitService]() {
            fmt::print("Timer callback\n");
            uint64_t i = co_await awaitService->waitOneSecond(); // the callback goes into waiting here, but other events can still be processed
            fmt::print("Waiting finished, ret {}\n", i);
            co_return {};
        });
        timer.startTimer();
        
        auto &timer2 = factory->createTimer();
        timer2.setChronoInterval(std::chrono::milliseconds(500));
        timer2.setCallbackAsync([]() {
            fmt::print("Timer2 callback\n"); // this will print a couple times until 
            co_return {};
        });
        timer2.startTimer();
    }
};
```

```c++
// main.cpp
#include <ichor/DependencyManager.h>
#include <ichor/event_queues/MultimapQueue.h>
#include <ichor/services/timer/TimerFactoryFactory.h>
#include "AwaitService.h"
#include "MyCoroutineTimerService.h"

int main() {
    auto queue = std::make_unique<MultimapQueue>();
    auto &dm = queue->createManager();
    dm.createServiceManager<AwaitService, IAwaitService>();
    dm.createServiceManager<MyCoroutineTimerService>();
    dm.createServiceManager<TimerFactoryFactory>();
    dm.start();
    
    return 0;
}
```

### Quitting the program

At some point in your program, the only thing left to do is tell Ichor to stop. This can easily be done by pushing a `QuitEvent`, like so:


```c++
// MyQuittingTimerService.h
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/event_queues/IEventQueue.h>

struct MyQuittingTimerService final {
    MyQuittingTimerService(ITimerFactory *factory, IEventQueue *queue) {
        auto &timer = factory->createTimer();
        timer.setChronoInterval(std::chrono::seconds(1));
        timer.setCallback([queue]() {
            queue->pushEvent<QuitEvent>(0);
        });
        timer.startTimer();
    }
};
```

And there you have it, the basic building blocks of Ichor!

## Advanced Features

There are a couple more advanced features that Ichor has:

### Listening to dependency requests

The biggest reason that Ichor is a runtime dependency manager rather than compile time, unlike say Boost.DI, is to have the option to decide which services to create, at runtime.
The number one use case here is to create a per-service-instance specific logger. Let's try to create a smaller `LoggerFactory` than is included in Ichor:

```c++
// includes and so on omitted for brevity
struct ILogger {
    virtual void Log(std::string_view msg) = 0;
};

struct Logger final : public ILogger {
    void Log(std::string_view msg) final {
        fmt::print("{}\n", msg);
    }
};

struct LoggerFactory final {
    LoggerFactory(DependencyManager *dm, IService *self) : _dm(dm) {
        _loggerTrackerRegistration = _dm->registerDependencyTracker<ILogger>(this, self);
    }

    void handleDependencyRequest(AlwaysNull<ILogger*>, DependencyRequestEvent const &evt) {
        auto logger = _loggers.find(evt.originatingService);

        if (logger != end(_loggers)) {
            return; // already created a logger for this service!
        }


        Properties props{};
        // Filter is a special property in Ichor, if this is detected, it gets checked before asking another service if they're interested in it.
        // In this case, we apply a filter specifically so that the requesting service id is the only one that will match.
        props.template emplace<>("Filter", Ichor::make_any<Filter>(ServiceIdFilterEntry{evt.originatingService}));
        auto *newLogger = _dm->createServiceManager<Logger, ILogger>(std::move(props));
        _loggers.emplace(evt.originatingService, newLogger);
    }

    void handleDependencyUndoRequest(AlwaysNull<ILogger*>, DependencyUndoRequestEvent const &evt) {
        auto service = _loggers.find(evt.originatingService);
        if(service != end(_loggers)) {
            _dm->pushEvent<StopServiceEvent>(AdvancedService<LoggerFactory<LogT>>::getServiceId(), service->second->getServiceId());
            // + 11 because the first stop triggers a dep offline event and inserts a new stop with 10 higher priority.
            _dm->pushPrioritisedEvent<RemoveServiceEvent>(AdvancedService<LoggerFactory<LogT>>::getServiceId(), INTERNAL_EVENT_PRIORITY + 11, service->second->getServiceId());
            _loggers.erase(service);
        }
    }
    
    DependencyTrackerRegistration _loggerTrackerRegistration{};
    unordered_map<uint64_t, IService*> _loggers;
    DependencyManager *_dm;
};

struct SomeServiceUsingLogger final {
    SomeServiceUsingLogger(ILogger *logger) {
        logger->Log("Logged!");
    }
};

int main() {
    auto queue = std::make_unique<MultimapQueue>();
    auto &dm = queue->createManager();
    dm.createServiceManager<LoggerFactory>(); // LoggerFactory will end up resolving the ILogger request from SomeServiceUsingLogger
    dm.createServiceManager<SomeServiceUsingLogger>();
    queue->start(CaptureSigInt);
    return 0;
}
```

### Interceptors

It is possible to intercept all events before they're handled by registered services (or Ichor itself!). An example:

```c++
// MyInterceptorService.h
#include <ichor/DependencyManager.h>
#include <ichor/events/Events.h>
#include <ichor/events/RunFunctionEvent>

struct MyInterceptorService final {
    MyInterceptorService(DependencyManager *dm, IService *self) {
        _interceptor = dm->registerEventInterceptor<Ichor::RunFunctionEvent>(this, self); // Can change RunFunctionEvent to just Event if you want to intercept *all* events
    }
    
    bool preInterceptEvent(Ichor::RunFunctionEvent const &) {
        return AllowOthersHandling; //PreventOthersHandling if this event should be discarded
    }
    
    void postInterceptEvent(Ichor::RunFunctionEvent const &, bool processed) {
        // Can use this to track how long the processing took
    }

    Ichor::EventInterceptorRegistration _interceptor{};
};
```

### Defining your own event

Events are easy to add, they need a constexpr TYPE and NAME and some fields as required by the constructor of Event. For the rest you're free to add any fields you like (though your event needs to be creatable by std::unique_ptr).
Your events can then be inserted, intercepted or handled as you would e.g. a `QuitEvent`.

```c++
struct MyEvent final : public Event {
    MyEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, uint64_t _someData) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority), someData(_someData) {}
    ~MyEvent() final = default;

    uint64_t someData;
    static constexpr uint64_t TYPE = typeNameHash<MyEvent>();
    static constexpr std::string_view NAME = typeName<MyEvent>();
};

// inserting them is easy
dm->pushEvent<MyEvent>(0, 5); //creates a MyEvent with a unique id, 0 as the originating service, standard priority and 5 as someData.

struct MyHandleService final {
    MyHandleService(DependencyManager *dm, IService *self) {
        _handler = dm->registerEventHandler<MyEvent>(this, self); // Can change RunFunctionEvent to just Event if you want to intercept *all* events
    }
    
    AsyncGenerator<IchorBehaviour> handleEvent(MyEvent const &e) {
        fmt::print("Handling MyEvent {}\n", e.someData); // prints 5, if using the insertion above.
        return AllowOthersHandling; //PreventOthersHandling if this event should not be handled by other event handlers
    }

    Ichor::EventHandlerRegistration _handler{};
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
    Ichor::AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
        getManager().getCommunicationChannel()->broadcastEvent<QuitEvent>(getManager(), getServiceId()); // sends to all other managers, except the one this service is registered to
        co_return {};
    }
};
```

The communication channel also has a `sendEventTo` function, which allows sending to a specific manager. Manager IDs are deterministic, the ID starts at 0 and increments by one for every created manager. See the comments above for `main.cpp` for an example.  
