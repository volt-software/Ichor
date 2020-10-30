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

struct ISigIntService : virtual public IService {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};
class SigIntService final : public ISigIntService, public Service {
public:
    bool start() final {
        // Setup a timer that fires every 10 milliseconds and tell that dependency manager that we're interested in the events that the timer fires.
        auto timer = getManager()->createServiceManager<Timer, ITimer>();
        timer->setChronoInterval(std::chrono::milliseconds(10));
        // Registering an event handler requires 2 pieces of information: the service id and a pointer to a service instantiation.
        // The third piece of information is an extra filter, as we're only interested in events of this specific timer.
        _timerEventRegistration = getManager()->registerEventHandler<TimerEvent>(getServiceId(), this, timer->getServiceId());
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
    std::locale::global(std::locale("en_US.UTF-8")); // some framework logging requires a proper locale

    DependencyManager dm{};
    // Register a framework logger and our sig int service.
    dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>();
    dm.createServiceManager<SigIntService, ISigIntService>();
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

Optional services:
* Websocket service through Boost.BEAST
* Spdlog logging service
* TCP communication service
* RapidJson serialization services
* Timer service
* Partial etcd service

# Roadmap

* Http server/client
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
* Start best case scenario: 10,000 starts with dependency in ~1,000 ms
* Start/stop best case scenario: 1,000,000 start/stops with dependency in ~840 ms
* Serialization: Rapidjson: 1,000,000 messages serialized & deserialized in ~350 ms

# Support

Feel free to make issues/pull requests and I'm sometimes online on Discord, though there's no 24/7 support: https://discord.gg/r9BtesB

# License

Ichor is licensed under the MIT license.
