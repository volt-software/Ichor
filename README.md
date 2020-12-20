# What is this?

Ichor, [Greek Mythos for ethereal fluid that is the blood of the gods/immortals](https://en.wikipedia.org/wiki/Ichor), is a C++ software suite to both quickly develop and easily refactor large back-end systems. 

Ichor informally stands for "Intuitive Compile-time Hoisted Object Resources".

Although initially started as a rewrite of OSGI-based framework Celix and to a lesser extent CppMicroservices, Ichor has carved out its own path, as god-fluids are wont to do. 

### Quickstart

The minimal example requires a main function, which initiates at least one event loop, a framework logger and one service. This is an example on how to quit the program on ctrl+c:

```c++
#include <ichor/DependencyManager.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include <ichor/optional_bundles/logging_bundle/CoutFrameworkLogger.h>
#include <ichor/optional_bundles/timer_bundle/TimerService.h>

using namespace Ichor;

std::atomic<bool> quit{};

void siginthandler(int param) {
    quit = true;
}

class SigIntService final : public Service {
public:
    bool start() final {
        // Setup a timer that fires every 10 milliseconds and tell that dependency manager that we're interested in the events that the timer fires.
        auto timer = getManager()->createServiceManager<Timer, ITimer>();
        timer->setChronoInterval(std::chrono::milliseconds(10));
        // Registering an event handler requires 2 pieces of information: the service id and a pointer to a service instantiation.
        // The third piece of information is an extra filter, as we're only interested in events of this specific timer.
        _timerEventRegistration = getManager()->registerEventHandler<TimerEvent>(this, timer->getServiceId());
        timer->startTimer();

        // Register sigint handler
        signal(SIGINT, siginthandler);
        return true;
    }

    bool stop() final {
        _timerEventRegistration = nullptr;
        return true;
    }

    Generator<bool> handleEvent(TimerEvent const * const evt) {
        // If sigint has been fired, send a quit to the event loop.
        // This can't be done from within the handler itself, as the mutex surrounding pushEvent might already be locked, resulting in a deadlock!
        if(quit) {
            getManager()->pushEvent<QuitEvent>(getServiceId());
        }
        co_return (bool)PreventOthersHandling;
    }

    std::unique_ptr<EventHandlerRegistration> _timerEventRegistration{nullptr};
}


int main() {
    std::locale::global(std::locale("en_US.UTF-8")); // some loggers require having a locale

    DependencyManager dm{};
    // Register a framework logger and our sig int service.
    dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
    dm.createServiceManager<SigIntService>();
    // Start manager, consumes current thread.
    dm.start();

    return 0;
}
```

More examples can be found in the examples directory.

### Dependencies

#### Ubuntu 20.04:

```
sudo add-apt-repository ppa:ubuntu-toolchain-r/ppa
sudo apt update
sudo apt install g++-10 build-essential cmake
```

Some features are behind feature flags and have their own dependencies.

If using etcd:
```
sudo apt install libgrpc++-dev libprotobuf-dev
```

If using the Boost.BEAST (recommended boost 1.70 or newer):
```
sudo apt install libboost1.71-all-dev libssl-dev
```

#### Windows

Untested, latest MSVC should probably work.

#### Mac

Untested, unknown if compiler supports enough C++20.

### Documentation

Documentation is rather...lacking at the moment. Contributions are very welcome!

### Current design focuses

* Less magic configuration
    * code as configuration, as much as possible bundled in one place
* As much type-safety as possible, prefer compile errors over run-time errors.
* Less multi-threading to prevent data races and similar issues
    * Use of an event loop
    * Where multi-threading is desired, provide easy to use abstractions to prevent issues
* Performance-oriented design in all-parts of the framework / making it easy to get high performance and low latency
* Fully utilise OOP, RAII and C++20 Concepts to steer users to using the framework correctly
* Implement business logic in the least amount of code possible 
* Hopefully this culminates and less error-prone code and better time to market 

### Supported features

The framework provides several core features and optional services behind cmake feature flags:
* Coroutine-based event loop
* Event-based message passing
* Dependency Management
* Service lifecycle management (sort of like OSGi-lite services)
* User-space priority-based real-time capable scheduling
* data race free communication between event loops
* Http server/client

Optional services:
* Websocket service through Boost.BEAST
* Spdlog logging service
* TCP communication service
* RapidJson serialization services
* Timer service
* Partial etcd service

# Roadmap

* CMake stuff to include ichor library from external project
* expand etcd support, currently only simply put/get supported
* Pubsub interfaces
    * Kafka? Pulsar? Ecal?
* Shell Commands
* Tracing interface
    * Opentracing? Jaeger?
* Docker integration/compilation
* ...

# Preliminary benchmark results
These benchmarks are mainly used to identify bottlenecks, not to showcase the performance of the framework. Proper throughput and latency benchmarks are TBD.

Setup: AMD 3900X, 3600MHz@CL17 RAM, ubuntu 20.04
* 1 thread inserting ~5 million events and then processing them in ~1,220 ms and ~965 MB memory usage
* 8 threads inserting ~5 million events and then processing them in ~2,020 ms and ~7,691 MB memory usage
* 1 thread creating 10,000 services with dependencies in ~6,262 ms and ~40 MB memory usage
* 8 threads creating 10,000 services with dependencies in ~25,187 ms and ~308 MB memory usage
* 1 thread starting/stopping 1 service 10,000 times in ~958 ms and ~4 MB memory usage
* 8 threads starting/stopping 1 service 10,000 times in ~1,829 ms and ~5 MB memory usage
* 1 thread serializing & deserializing 1,000,000 JSON messages in ~410 ms and ~4 MB memory usage
* 8 threads serializing & deserializing 1,000,000 JSON messages in ~495 ms and ~5 MB memory usage

These benchmarks currently lead to the characteristics:
* creating services with dependencies overhead is likely O(N²).
* Starting services, stopping services overhead is likely O(N)
* event handling overhead is amortized O(1)
* Creating more threads is not 100% linearizable in all cases (pure event creation/handling seems to be, otherwise not really)

Help with improving memory usage and the O(N²) behaviour would be appreciated.

# Support

Feel free to make issues/pull requests and I'm sometimes online on Discord, though there's no 24/7 support: https://discord.gg/r9BtesB

# License

Ichor is licensed under the MIT license.
