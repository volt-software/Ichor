# This page is outdated, please read the Getting Started doc until I have the time to update this. 
# Fundamentals

Ichor is a combination of three components:

* Dependency Injection (DI)
* Service Lifecycle Management (SLM)
* Event Queue (EQ)

Using the latest C++20 features:
* Coroutines
* Concepts

## Dependency Injection (DI)

Ichor's DI is run-time. There's some compile time stuff to determine which interfaces are requested, but resolving and registering happens at run-time.
The main feature Ichor aims for is run-time interception of dependency requests and creating a new dependency instance specifically for that request.
One of the use-cases is to easily setup factories separated per function domain. E.g. to create two workflows which share common code but separate data. 
Another would be to dynamically load .so or .dll files which expose common Ichor dependencies, to be easily shared accross multiple projects.

### Requesting Dependencies

```c++
struct ISomeDependency{};
struct ISomeOptionalDependency{};
struct DependencyService final : public Service<DependencyService> {
    DependencyService(DependencyRegister &reg, Properties props, DependencyManager *) : Service(std::move(props), mng) {
        reg.registerDependency<ISomeDependency>(this, DependencyFlags::REQUIRED /* required dependency */);
        reg.registerDependency<ISomeOptionalDependency>(this, DependencyFlags::NONE /* optional dependency */);
    }
    ~DependencyService() final = default;

    void addDependencyInstance(ISomeDependency * /* Injected service */, IService * /* service interface for injected service, e.g. service id and properties */) {}
    void removeDependencyInstance(ISomeDependency *, IService *) {}
    void addDependencyInstance(ISomeOptionalDependency *, IService *) {}
    void removeDependencyInstance(ISomeOptionalDependency *, IService *) {}
};
```

### Other Dependency Injection frameworks

Purely comparing on DI, Ichor doesn't provide a very compelling alternative.
It is only when also taking Service Lifecyce Management and the thread safety guarantees in consideration that Ichor fills a need. Regardless, here are a couple of other frameworks:

#### Boost.DI

Boost.DI is a purely compile-time DI framework. This enables it to have no run-time overhead, but doesn't allow creating a new instance per dependency request. It requires all possible interfaces and how to instantiate them to be known at compile time.

#### Google Fruit

Fruit generally provides more features than Ichor does. Fruit also supports [Dependency factories](https://github.com/google/fruit/wiki/quick-reference#factories-and-assisted-injection). However, thread-safety is something that is left to the user.

## Service Lifecycle Management (SLM)

On top of DI, Ichor gives class instances the ability to start/stop themselves without removing the instance out of memory.
This is especially useful in resource-constrained systems where before going OOM, an application might want to shut down parts of its memory and scale up ASAP when the memory pressure is lower.  

### Required / Optional Dependencies

Most DI use-cases revolve around connecting interfaces to an implementation and simply start the implementation when all dependencies are available.
Ichor provides optional dependencies, which allows for dealing with multiple instances. An example is the SerializerAdmin given in the optional bundles.
Registering a serializer for a specific type is as simple as starting a new instance implementing ISerializer. The admin will scoop it up and use it whenever it receives a request to serialize that type.

## Event Queue (EQ)

Tying the concept of thread-safety together with both DI and SLM means that effectively, there's one Dependency Manager per thread. Services, dependencies nor any kind of memory is shared between threads.

### No shared data

### Ensuring determinism

Although in normal usage, order of events is determined by which time it enters into the queue, in simulated usage this can be manipulated to always lead to the same event order.

Ichor, at least on Linux, ensures that the highest priority thread trying to insert an event gets in first.

### Real-time scheduling

When combined with a real-time kernel, each thread can be run with a real-time guarantee. Although Ichor provides the concept of priorities, these are discouraged in real-time usage. Most real-time tasks run in a linear fashion so that doesn't create issues.

### Supported Out-Of-The-Box

Ichor provides a multimap-based priority queue as well as an [sdevent](https://www.freedesktop.org/software/systemd/man/sd-event.html) implementation out of the box. Custom ones can be made to suit your needs.
The sdevent implementation is a showcase on how to implement Ichor on top of your existing event queue.

## C++20

### Coroutines

Ichor provides a co_yield and co_await capable generator for use in conjunction with its event queue integration. An example:

```c++
AsyncManualResetEvent evt;
dm.pushEvent<RunFunctionEventAsync>(0, [&](DependencyManager& mng) -> AsyncGenerator<IchorBehaviour> {
    co_await evt;
    co_return;
});

// ... Sometime Later ...

dm.pushEvent<RunFunctionEvent>(0, [&](DependencyManager& mng) {
    evt.set();
});
```

What happens under the hood is that a `RunFunctionEventAsync` gets inserted into the queue and executed. Upon encountering a `co_await`, a coroutine handle gets created and stored in Ichor.

Sometime later, a non-coroutine based `RunFunctionEvent` is inserted and executed, which sets the Async event, immediately continuing the execution of the previous coroutine, after which its execution is finished.

Big thanks for Lewis Baker's [cppcoro](https://github.com/lewissbaker/cppcoro), which Ichor borrowed heavily from.

For more details see the `sendTestRequest` function in `UsingHttpService.h` in the [http example](../examples/http_example)

### Concepts

For registering dependencies, trackers, http routes and more, Ichor uses templated with concepts to ensure the right function signatures are present. These then get stored in Ichor using type-erasure through `std::function`.
