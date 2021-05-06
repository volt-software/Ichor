[![Build Status](https://travis-ci.com/volt-software/Ichor.svg?branch=master)](https://travis-ci.com/volt-software/Ichor)

# What is this?

Ichor, [Greek Mythos for ethereal fluid that is the blood of the gods/immortals](https://en.wikipedia.org/wiki/Ichor), is a C++ framework/middleware for thread confinement and dependency injection. 

Ichor informally stands for "Intuitive Compile-time Hoisted Object Resources".

Although initially started as a rewrite of OSGI-based framework Celix and to a lesser extent CppMicroservices, Ichor has carved out its own path, as god-fluids are wont to do. 

Ichor's greater aim is to bring C++20 to the embedded realm. This means that classic requirements such as the possibility to have 0 dynamic memory allocations, control over scheduling and deterministic execution times are a focus, whilst still allowing much of standard modern C++ development. 

Moreover, the concept of [Fearless Concurrency](https://doc.rust-lang.org/book/ch16-00-concurrency.html) from Rust is attempted to be applied in Ichor through thread confinement.   

### Thread confinement? Fearless Concurrency?

Multithreading is hard. There exist plenty of methods trying to make it easier, ranging from the [actor framework](https://github.com/actor-framework/actor-framework), [static analysis a la rust](https://doc.rust-lang.org/book/ch16-00-concurrency.html), [software transaction memory](https://en.wikipedia.org/wiki/Software_transactional_memory) and traditional manual lock-wrangling.

Thread confinement is one such approach. Instead of having to protect resources, Ichor attempts to make it well-defined on which thread an instance of a C++ class runs and pushes you to only access and modify memory from that thread. Thereby removing the need to think about atomics/mutexes, unless you use threads not managed by, or otherwise trying to circumvent, Ichor.
In which case, you're on your own.

## Quickstart

The [minimal example](examples/minimal_example/main.cpp) requires a main function, which initiates at least one event loop, a framework logger and one service and quitting the program gracefully using ctrl+c.

The [realtime example](examples/realtime_example/main.cpp) shows a trivial program running with realtime priorities and shows some usage of Ichor priorities.

More examples can be found in the [examples directory](examples).

## Supported OSes
* Linux

## Currently Unsupported
* Windows, untested, not sure if compiles but should be relatively easy to get up and running
* Baremetal, might change if someone puts in the effort to modify Ichor to work with freestanding implementations of C++20
* Far out future plans for any RTOS that supports C++20 such as VxWorks Wind River

## Dependencies

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

#### Windows

Untested, latest MSVC should probably work.

## Documentation

Documentation can be found in the [docs directory](docs).

### Current design focuses

* Less magic configuration
    * code as configuration, as much as possible bundled in one place
* As much type-safety as possible, prefer compile errors over run-time errors.
* Well-defined and managed multi-threading to prevent data races and similar issues
    * Use of an event loop
    * Where multi-threading is desired, provide easy to use abstractions to prevent issues
* Performance-oriented design in all-parts of the framework / making it easy to get high performance and low latency
* Usage of memory allocators, enabling 0 heap allocation C++ usage.
* Fully utilise OOP, RAII and C++20 Concepts to steer users to using the framework correctly
* Implement business logic in the least amount of code possible 
* Hopefully this culminates and less error-prone code and better time to market 

### Supported features

The framework provides several core features and optional services behind cmake feature flags:
* Coroutine-based event loop
* Event-based message passing
* Dependency Injection
* Memory allocators for 0 heap allocations
* Service lifecycle management (sort of like OSGi-lite services)
* data race free communication between event loops
* Http server/client

Optional services:
* Websocket service through Boost.BEAST
* HTTP client and server services through Boost.BEAST
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
* 1 thread inserting ~5 million events and then processing them in ~1,353 ms and ~647 MB memory usage
* 8 threads inserting ~5 million events and then processing them in ~1,938 ms and ~5,137 MB memory usage
* 1 thread creating 10,000 services with dependencies in ~3,8000 ms and ~33 MB memory usage
* 8 threads creating 10,000 services with dependencies in ~12,200 ms and ~237 MB memory usage
* 1 thread starting/stopping 1 service 10,000 times in ~1,193 ms and ~4 MB memory usage
* 8 threads starting/stopping 1 service 10,000 times in ~2,480 ms and ~5 MB memory usage
* 1 thread serializing & deserializing 1,000,000 JSON messages in ~380 ms and ~4 MB memory usage
* 8 threads serializing & deserializing 1,000,000 JSON messages in ~400 ms and ~5 MB memory usage

Realtime example on a vanilla linux:
```
root# echo 950000 > /proc/sys/kernel/sched_rt_runtime_us #force kernel to fail our deadlines
root# ./ichor_realtime_example 
duration of run 4,076 is 51,298 µs which exceeded maximum of 2,000 µs
duration of run 6,750 is 50,296 µs which exceeded maximum of 2,000 µs
duration of run 9,396 is 50,293 µs which exceeded maximum of 2,000 µs
duration of run 12,055 is 50,296 µs which exceeded maximum of 2,000 µs
duration of run 14,719 is 50,295 µs which exceeded maximum of 2,000 µs
duration of run 17,374 is 50,297 µs which exceeded maximum of 2,000 µs
duration min/max/avg: 349/51,298/371 µs
root#
root# echo 999999 > /proc/sys/kernel/sched_rt_runtime_us
root# ./ichor_realtime_example 
duration min/max/avg: 179/838/204 µs
root#
root# echo -1 > /proc/sys/kernel/sched_rt_runtime_us
root# ./ichor_realtime_example 
duration min/max/avg: 274/368/276 µs
```

These benchmarks currently lead to the characteristics:
* creating services with dependencies overhead is likely O(N²).
* Starting services, stopping services overhead is likely O(N)
* event handling overhead is amortized O(1)
* Creating more threads is not 100% linearizable in all cases (pure event creation/handling seems to be, otherwise not really)
* Latency spikes while scheduling in user-space is in the order of 100's of microseconds, lower if a proper [realtime tuning guide](https://rigtorp.se/low-latency-guide/) is used (except if the queue goes empty and is forced to sleep)

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

### Windows? OS X? VxWorks Wind River? Baremetal?

I don't have a machine with windows/OS X to program for (and also know if there is much demand for it), so I haven't started on it.

What is necessary to implement before using Ichor on these platforms:
* Ichor [STL](include/ichor/stl) functionality, namely the RealtimeMutex and ConditionVariable.
* Compiler support for C++20 may not be adequate yet.

The same goes for Wind River. Freestanding implementations might be necessary for Baremetal support, but that would stray rather far from my expertise.

### Why re-implement parts of the STL?

To add support for memory allocators and achieve 0 dynamic memory allocation support. Particularly std::any and the (read/write) mutexes.

But also because the real-time extensions to mutexes (PTHREAD_MUTEX_ADAPTIVE_NP/PTHREAD_PRIO_INHERIT/PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP) is either not a standard extension or not exposed by the standard library equivalents.

# License

Ichor is licensed under the MIT license.
