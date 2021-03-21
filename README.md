# What is this?

Ichor, [Greek Mythos for ethereal fluid that is the blood of the gods/immortals](https://en.wikipedia.org/wiki/Ichor), is a C++ framework/middleware for thread confinement and dependency management. 

Ichor informally stands for "Intuitive Compile-time Hoisted Object Resources".

Although initially started as a rewrite of OSGI-based framework Celix and to a lesser extent CppMicroservices, Ichor has carved out its own path, as god-fluids are wont to do. 

Ichor's greater aim is to bring C++20 to the embedded realm. This means that classic requirements such as the possibility to have 0 dynamic memory allocations, control over scheduling and deterministic execution times are a focus, whilst still allowing much of standard modern C++ development. 

Moreover, the concept of [Fearless Concurrency](https://doc.rust-lang.org/book/ch16-00-concurrency.html) from Rust is attempted to be applied in Ichor through thread confinement.   

### Thread confinement? Fearless Concurrency?

Multithreading is hard. There exist plenty of methods trying to make it easier, ranging from the [actor framework](https://github.com/actor-framework/actor-framework), [static analysis a la rust](https://doc.rust-lang.org/book/ch16-00-concurrency.html), [software transaction memory](https://en.wikipedia.org/wiki/Software_transactional_memory) and traditional manual lock-wrangling.

Thread confinement is one such approach. Instead of having to protect resources, Ichor attempts to make it well-defined on which thread an instance of a C++ class runs and only allows modification to the memory from that thread. Thereby removing the need to think about atomics/mutexes, unless you use threads not managed by Ichor.

## Quickstart

The [minimal example](examples/minimal_example/main.cpp) requires a main function, which initiates at least one event loop, a framework logger and one service and quitting the program gracefully using ctrl+c.

The [realtime example](examples/realtime_example/main.cpp) shows a trivial program running with realtime priorities and shows some usage of Ichor priorities.

More examples can be found in the [examples directory](examples).


## Dependencies

### Ubuntu 20.04:

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

## Documentation

Documentation is rather...lacking at the moment. Contributions are very welcome!

### Current design focuses

* Less magic configuration
    * code as configuration, as much as possible bundled in one place
* As much type-safety as possible, prefer compile errors over run-time errors.
* Well-defined and managed multi-threading to prevent data races and similar issues
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

* EDF scheduling / WCET measurements
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
* 1 thread inserting ~5 million events and then processing them in ~945 ms and ~965 MB memory usage
* 8 threads inserting ~5 million events and then processing them in ~1,424 ms and ~7,691 MB memory usage
* 1 thread creating 10,000 services with dependencies in ~4,900 ms and ~40 MB memory usage
* 8 threads creating 10,000 services with dependencies in ~16,230 ms and ~308 MB memory usage
* 1 thread starting/stopping 1 service 10,000 times in ~1,126 ms and ~4 MB memory usage
* 8 threads starting/stopping 1 service 10,000 times in ~1,895 ms and ~5 MB memory usage
* 1 thread serializing & deserializing 1,000,000 JSON messages in ~343 ms and ~4 MB memory usage
* 8 threads serializing & deserializing 1,000,000 JSON messages in ~375 ms and ~5 MB memory usage

Realtime example on a vanilla linux:
```
duration of run 5,174 is 50,774 µs which exceeded maximum of 2,000 µs
duration of run 8,584 is 49,804 µs which exceeded maximum of 2,000 µs
duration of run 11,278 is 50,833 µs which exceeded maximum of 2,000 µs
duration of run 13,977 is 49,847 µs which exceeded maximum of 2,000 µs
duration of run 16,675 is 50,893 µs which exceeded maximum of 2,000 µs
duration of run 19,340 is 49,830 µs which exceeded maximum of 2,000 µs
duration min/max/avg: 274/50,893/335 µs
```

Realtime example on an RT_PREEMPT linux, [tuned](https://rigtorp.se/low-latency-guide/):
```
root@xanthos:/home/oipo-unencrypted/Programming/cppelix/bin# ./ichor_realtime_example 
duration of run 6,013 is 49,191 µs which exceeded maximum of 2,000 µs
duration of run 11,215 is 50,187 µs which exceeded maximum of 2,000 µs
duration of run 16,420 is 50,187 µs which exceeded maximum of 2,000 µs
duration min/max/avg: 177/50,187/188 µs
```

These benchmarks currently lead to the characteristics:
* creating services with dependencies overhead is likely O(N²).
* Starting services, stopping services overhead is likely O(N)
* event handling overhead is amortized O(1)
* Creating more threads is not 100% linearizable in all cases (pure event creation/handling seems to be, otherwise not really)

Help with improving memory usage and the O(N²) behaviour would be appreciated.

# Support

Feel free to make issues/pull requests and I'm sometimes online on Discord: https://discord.gg/r9BtesB

Business inquiries can be sent to michael AT volt-software.nl

# FAQ

### I want to have a non-voluntary pre-emption scheduler

Currently Ichor uses a mutex when inserting/extracting events from its queue. Because non-voluntary user-space scheduling requires lock-free data-structures, this is not possible yet.

### Is it possible to have a completely stack-based allocation while using C++?

Yes, see the [realtime example](examples/realtime_example). This example terminates the program whenever there is a dynamic memory allocation, aside from some allowed exceptions like the std::locale allocation.

### I'd like to use clang

To my knowledge, clang doesn't have certain C++20 features used in Ichor yet. As soon as clang implements those, I'll work on adding support.

### Windows? OS X?

I don't have a machine with windows/OS X to program for (and also know if there is much demand for it), so I haven't started on it.

What is necessary to implement before using Ichor on these platforms:
* Ichor [STL](include/ichor/stl) functionality, namely the RealtimeMutex and ConditionVariable.
* Compiler support for C++20 may not be adequate yet.

### Why re-implement parts of the STL?

To add support for memory allocators and achieve 0 dynamic memory allocation support. Particularly std::any.

But also because the real-time extensions to mutexes (PTHREAD_MUTEX_ADAPTIVE_NP/PTHREAD_PRIO_INHERIT) is either not a standard extension or not exposed by std::mutex.

# License

Ichor is licensed under the MIT license.
