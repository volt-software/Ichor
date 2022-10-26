![example workflow](https://github.com/volt-software/Ichor/actions/workflows/cmake.yml/badge.svg)

# What is this?

Ichor, [Greek Mythos for ethereal fluid that is the blood of the gods/immortals](https://en.wikipedia.org/wiki/Ichor), is a C++ framework/middleware for thread confinement and dependency injection. 

Ichor informally stands for "Intuitive Compile-time Hoisted Object Resources".

Ichor's greater aim is to support C++ projects with long software lifecycles. Think 10-40 years. By decoupling interface and implement, library updates or changes to user requirements are confined to a smaller area.  

Moreover, the concept of [Fearless Concurrency](https://doc.rust-lang.org/book/ch16-00-concurrency.html) from Rust is attempted to be applied in Ichor through thread confinement.   

### Thread confinement? Fearless Concurrency?

Multithreading is hard. There exist plenty of methods trying to make it easier, ranging from the [actor framework](https://github.com/actor-framework/actor-framework), [static analysis a la rust](https://doc.rust-lang.org/book/ch16-00-concurrency.html), [software transaction memory](https://en.wikipedia.org/wiki/Software_transactional_memory) and traditional manual lock-wrangling.

Thread confinement is one such approach. Instead of having to protect resources, Ichor attempts to make it well-defined on which thread an instance of a C++ class runs and pushes you to only access and modify memory from that thread. Thereby removing the need to think about atomics/mutexes, unless you use threads not managed by, or otherwise trying to circumvent, Ichor.
In which case, you're on your own.

### Dependency Injection?

To support software lifecycles measured in decades, engineers try to encapsulate functionality. Using the Dependency Injection approach allows engineers to insulate updates to parts of the code.

There exist many Dependency Injection libraries for C++ already, but where those usually only provide Dependency Injection, Ichor also provides service lifecycle management and thread confinement. If a dependency goes away at runtime, e.g. a network client, then all the services depending on it will be cleaned up at that moment. 

## Quickstart

The [minimal example](examples/minimal_example/main.cpp) requires a main function, which initiates at least one event loop, a framework logger and one service and quitting the program gracefully using ctrl+c.

The [realtime example](examples/realtime_example/main.cpp) shows a trivial program running with realtime priorities and shows some usage of Ichor priorities.

More examples can be found in the [examples directory](examples).

## Supported OSes
* Linux

## Supported Compilers
* Gcc 11.3 or newer (see [this gcc bug](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=95137) for why)
* Clang 14 or newer

## Currently Unsupported
* Windows, untested, not sure if compiles but should be relatively easy to get up and running
  * Tried with MSVC 19.33, but it seemed like coroutines and concepts are not fully implemented yet
* Baremetal, might change if someone puts in the effort to modify Ichor to work with freestanding implementations of C++20
* Far out future plans for any RTOS that supports C++20 such as VxWorks Wind River, FreeRTOS

## Dependencies

### Ubuntu 20.04:

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

### Ubuntu 22.04:

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

#### Windows

Tried with MSVC 19.33, but it seemed like coroutines and concepts are not fully implemented yet.

## Building

Linux:
```
git clone https://github.com/volt-software/Ichor.git
mkdir Ichor/build
cd Ichor/build
cmake ..
make -j$(nproc)
make test
../bin/ichor_minimal_example
(ctrl + c to quit)
```

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
* Fully utilise OOP, RAII and C++20 Concepts to steer users to using the framework correctly
* Implement business logic in the least amount of code possible 
* Hopefully this culminates and less error-prone code and better time to market 

### Supported features

The framework provides several core features and optional services behind cmake feature flags:
* Coroutine-based event loop
* Event-based message passing
* Dependency Injection
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
* expand/re-do etcd support, currently only simply put/get supported
* Pubsub interfaces
    * Kafka? Pulsar? Ecal?
* Shell Commands
* Tracing interface
    * Opentracing? Jaeger?
* Docker integration/compilation
* "Remote" services, where services are either in a different thread or a different machine
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

Ichor used to have full support for the polymorphic allocators, but as not all compilers support it yet (looking at you clang) as well as having a negative impact on developer ergonomy, it has been removed.
Instead, Ichor now recommends usage with [mimalloc](https://github.com/microsoft/mimalloc): "it does not suffer from blowup, has bounded worst-case allocation times (wcat), bounded space overhead (~0.2% meta-data, with low internal fragmentation), and has no internal points of contention using only atomic operations."

### Windows? OS X? VxWorks Wind River? Baremetal?

I don't have a machine with OS X to program for (and also don't know if there is much demand for it), so I haven't started on it.

What is necessary to implement before using Ichor on these platforms:
* Ichor [STL](include/ichor/stl) functionality, namely the RealtimeMutex and ConditionVariable.
* Compiler support for C++20 may not be adequate yet.

The same goes for Wind River. Freestanding implementations might be necessary for Baremetal support, but that would stray rather far from my expertise.

### Why re-implement parts of the STL?

To add support for `-fno-rtti` while providing the functionality of `std::any` and realtime mutexes (at least on linux).

The real-time extensions to mutexes (PTHREAD_MUTEX_ADAPTIVE_NP/PTHREAD_PRIO_INHERIT/PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP) are either not a standard extension or not exposed by the standard library equivalents.

# License

Ichor is licensed under the MIT license.
