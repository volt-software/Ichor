# Fundamentals

Ichor is a combination of three components:

* Dependency Injection (DI)
* Service Lifecycle Management (SLM)
* Event Queue (EQ)

## Dependency Injection (DI)

Ichor's DI is run-time. There's some compile time stuff to determine which interfaces are requested, but resolving and registering happens at run-time.
The main feature Ichor aims for is run-time interception of dependency requests and creating a new dependency instance specifically for that request.
One of the use-cases is to easily setup factories separated per function domain. E.g. to create two workflows which share common code but separate data. 

### Requesting Dependencies

```c++
struct ISomeDependency{};
struct ISomeOptionalDependency{};
struct DependencyService final : public Service<DependencyService> {
    DependencyService(DependencyRegister &reg, Properties props, DependencyManager *) : Service(std::move(props), mng) {
        reg.registerDependency<ISomeDependency>(this, true /* required dependency */);
        reg.registerDependency<ISomeOptionalDependency>(this, false /* optional dependency */);
    }
    ~DependencyService() final = default;

    void addDependencyInstance(ISomeDependency *, IService *) {}
    void removeDependencyInstance(ISomeDependency *, IService *) {}
    void addDependencyInstance(ISomeOptionalDependency *, IService *) {}
    void removeDependencyInstance(ISomeOptionalDependency *, IService *) {}
};
```

### Other Dependency Injection frameworks

Purely comparing on DI, Ichor doesn't provide a very compelling alternative.
It is only when also taking Service Lifecyce Management and the thread safety guarantees in consideration that Ichor fills a need. Regardless, here are a couple of other frameworks:

#### Boost.DI

Boost.DI is a purely compile-time DI framework. This enables it to have no run-time overhead, but doesn't allow creating a new instance per dependency request.

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