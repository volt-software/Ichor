![CI](https://github.com/volt-software/Ichor/actions/workflows/cmake.yml/badge.svg)
[![codecov](https://codecov.io/gh/volt-software/Ichor/branch/main/graph/badge.svg?token=ATIZ63PTFJ)](https://codecov.io/gh/volt-software/Ichor)

# What is this?

Ichor is mainly my own personal project to achieve the following things:
* Abstracting modern submission/completion queue approaches like io_uring and windows' IoRing, swapping approaches without needing to modify user code at all
* Add lifetimes and settings to individual instances of classes 
* Using modern C++ features like coroutines but provide very detailed control of debugging 
* Performance matching at least Boost Beast
* Memory and thread safety as much as possible using modern compiler flags and software approaches

Because I want to minimise coupling between classes and the underlying used technologies as much as possible, dependency injection is at the heart of Ichor. Second, because I want instances to have a lifetime, because I want to use coroutines and because I want to use io_uring, event queueing is the second heart of Ichor. Without either of the two, most of the framework will not form a cohesive whole.

### Boost supports event loops, dependency injection, networking, can do all of that, and more. Any reason i'd want to use your lib instead?

I've personally found Boost to have too many breaking changes. The approach I have with Ichor is to reduce the refactoring necessary when upgrading libraries as well as to have versioned code API, to support bridging old and new API without breaking existing code. Moreover, there's a strong vendor locking as the boost types will end up being present through your entire codebase. Ichor doesn't completely remove that, but reduces it as much as possible.

Second, although Boost provides an impressive amount of supported compilers and has tests, there's a couple of indicators I use that I want to score better on than Boost (currently not achieved yet). Code coverage provides a skewed perspective on quality, model based coverage such as states or risk based testing provides a better perspective, as such these are the metrics I want Ichor to score better no:

* API stability and ease of use
* State based coverage model
* Risk based testing
* Differential tests (comparing e.g. Ichor's redis implementation vs hiredis)
* Fuzzing (boost does this currently, Ichor does not)
* Compiler warnings treated as errors, using f.e. [Jason Turner's best practices](https://github.com/cpp-best-practices/cppbestpractices/blob/master/02-Use_the_Tools_Available.md#gcc%E2%80%94clang) and the most most [recent compiler hardening flags](https://best.openssf.org/Compiler-Hardening-Guides/Compiler-Options-Hardening-Guide-for-C-and-C++.html). Boost doesn't enable many by default, which in my eyes makes boost more error-prone and less secure.
* Performance: especially Boost.BEAST is not very performant in comparison to other libraries such as Lithium or Drogon. See [the techempowered benchmark](https://www.techempower.com/benchmarks/#hw=ph&test=fortune&section=data-r23&l=ziimf3-pa7) or [Ichor's own benchmarks](./benchmarks/README.md) to see how easy it is to beat Boost.BEAST performance-wise.

However, these all tell you more about what I find important. It's great to have options and if you find Boost working for you, then you should continue using it.

### Thread confinement? Fearless Concurrency?

Multithreading is hard. There exist plenty of methods trying to make it easier, ranging from the [actor framework](https://github.com/actor-framework/actor-framework), [static analysis a la rust](https://doc.rust-lang.org/book/ch16-00-concurrency.html), [software transaction memory](https://en.wikipedia.org/wiki/Software_transactional_memory) and traditional manual lock-wrangling.

Thread confinement is one such approach. Instead of having to protect resources, Ichor attempts to make it well-defined on which thread an instance of a C++ class runs and pushes you to only access and modify memory from that thread. Thereby removing the need to think about atomics/mutexes, unless you use threads not managed by, or otherwise trying to circumvent, Ichor.
In which case, you're on your own.

### Dependency Injection?

To support software lifecycles measured in decades, engineers try to encapsulate functionality. Using the [Dependency Injection](./docs/02-DependencyInjection.md) approach allows engineers to insulate updates to parts of the code.

There exist many Dependency Injection libraries for C++ already, but where those usually only provide Dependency Injection, Ichor also provides service lifecycle management and thread confinement. If a dependency goes away at runtime, e.g. a network client, then all the services depending on it will be cleaned up at that moment. 

## Quickstart

The [minimal example](examples/minimal_example/main.cpp) requires a main function, which initiates at least one event loop, a framework logger and one service and quitting the program gracefully using ctrl+c.

The [realtime example](examples/realtime_example/main.cpp) shows a trivial program running with realtime priorities and shows some usage of Ichor priorities.

More examples can be found in the [examples directory](examples).

## Supported OSes
* Linux (including aarch64 and musl-based distros like alpine linux)
* Windows
* Partial support for OSX Monterey (using `brew install llvm`, ASAN in combination with boost beast may result in false positives)

## Supported Compilers
* Gcc 12 or newer
* Clang 15 or newer
* MSVC 17.4+ (though some workarounds for [some](https://developercommunity.visualstudio.com/t/C2039:-promise_type:-is-not-a-member-o/10505491) [compiler](https://developercommunity.visualstudio.com/t/c20-Friend-definition-of-class-with-re/10197302) [bugs](https://developercommunity.visualstudio.com/t/certain-coroutines-cause-error-C7587:-/10311276?q=%22task+runner%22+gulp+duration&sort=newest) are used)

## Currently Unsupported
* Baremetal, might change if someone puts in the effort to modify Ichor to work with freestanding implementations of C++20
* Far out future plans for any RTOS that supports C++20 such as VxWorks Wind River, FreeRTOS

## Building

For build instructions and required dependencies, please see [GettingStarted](docs/01-GettingStarted.md).

## Documentation

Documentation can be found in the [docs directory](docs).

### Current design focuses

* code as configuration, as much as possible bundled in one place
* As much type-safety as possible, prefer compile errors over run-time errors.
* Well-defined and managed multi-threading to prevent data races and similar issues
    * Use of an event loop
    * Where multi-threading is desired, provide easy to use abstractions to prevent issues
* Performance-oriented design in all-parts of the framework / making it easy to get high performance and low latency
* Fully utilise OOP, RAII and C++20 Concepts to steer users to using the framework correctly
* Implement business logic in the least amount of code possible 
* Less error-prone code and better time to market 

### Supported features

The framework provides several core features and optional services behind cmake feature flags:
* Coroutine-based event loop
* Event-based message passing
* Dependency Injection
* Service lifecycle management (sort of like OSGi-lite services)
* data race free communication between event loops on multiple threads

Optional services:
* Websocket service through Boost.BEAST
* HTTP client and server services, partial implementation of HTTP/1.1, HTTPS only through boost Beast
* logging services
* TCP communication service through io_uring and Boost.ASIO
* JSON serialization services examples
* Timer service
* Redis service
* Etcd v2 & v3 service

# Roadmap

* EDF scheduling / WCET measurements
* Pubsub interfaces
    * Kafka? Pulsar? Ecal?
* Shell Commands / REPL
* Tracing interface
    * Opentracing? Jaeger?
* "Remote" services, where services are either in a different thread or a different machine
* Code generator to reduce boilerplate
* windows completion ports implementation?
* OSX kqueue implementation?
* ...

# Benchmarks

See [benchmarks directory](benchmarks)

# Support

Feel free to make issues/pull requests, and I'm sometimes online on Discord: https://discord.gg/r9BtesB

Business inquiries can be sent to michael AT volt-software.nl

# FAQ

### I want to have a non-voluntary pre-emption scheduler

By default, Ichor uses a mutex when inserting/extracting events from its queue. Because non-voluntary user-space scheduling requires lock-free data-structures, this is not possible.

### Is it possible to have a completely stack-based allocation while using C++?

Ichor used to have full support for the polymorphic allocators, but as not all compilers support it yet (looking at you libc++) as well as having a negative impact on developer ergonomy, it has been removed.
Instead, Ichor now recommends usage with [mimalloc](https://github.com/microsoft/mimalloc): 

> it does not suffer from blowup, has bounded worst-case allocation times (wcat), bounded space overhead (~0.2% meta-data, with low internal fragmentation), and has no internal points of contention using only atomic operations.

### FreeRTOS? VxWorks Wind River? Baremetal?

I'm only one guy, time is my most limited resource. But if anyone wants to open a PR:

What is necessary to implement before using Ichor on these platforms:
* Ichor [STL](include/ichor/stl) functionality, namely the RealtimeMutex and ConditionVariable.
* Compiler support for C++20 may not be adequate yet.
* Time / effort.

The same goes for Wind River. Freestanding implementations might be necessary for Baremetal support, but that would stray rather far from my expertise.

### Why re-implement parts of the STL?

* To add support for `-fno-rtti` while providing the functionality of `std::any` and realtime mutexes (at least on linux).
* The real-time extensions to mutexes (PTHREAD_MUTEX_ADAPTIVE_NP/PTHREAD_PRIO_INHERIT/PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP) are either not a standard extension or not exposed by the standard library equivalents.
* To support coroutines on mutexes
* To eliminate redundant checks when using unique_ptrs and priority queues
* VectorView allowing to return a span<PARENT>-like when you have a vector<CHILD>
* StaticVector implementation for C++20
* Some string conversion and split functions that are faster than std implementations
* StrongTypes-lite such as possible in libraries like [type_safe](https://github.com/foonathan/type_safe/tree/main) 

# License

Ichor is licensed under the MIT license.
