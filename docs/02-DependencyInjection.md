# Dependency Injection (DI)

Ichor heavily uses Dependency Injection to ensure easier long term maintainability as well as make it easier to modify Ichor to suit the users use-cases.

## What is DI?

DI is a design pattern that decouples the creation of objects from using them. Instead of knowing about objects/functions directly, they are 'hidden' behind a virtual interface and the code using the interface doesn't know anything about its internals.

The benefits of this pattern are generally:

* Refactoring costs go down in big projects
* Better seperation of concerns, i.e. easier to reason about code
* Easier to test parts of the code in isolation

Obviously, the cost is a bit of boilerplate.

There's a lot of resources on DI in C++ already, please consult the Extra Resources at the bottom of the page to get a more detailled explanation of this pattern.

## Why use DI in C++?

C++ users traditionally are looking to get as close to the metal as possible. Even going so far as to look down upon using virtual method calls, as that introduces vtable lookups, pointer indirection and prevents inlining. 

However, as compilers have been getting better (e.g. [LTO](https://en.wikipedia.org/wiki/Interprocedural_optimization)), processors get better at branch prediction and have more L1/L2/L3 cache, the impact of virtual methods have been reduced. Moreover, software developers don't need the maximum performance everywhere, just in hot code paths or there where the profiler shows a lot of CPU time.

Everywhere else, developers should prefer increasing development productivity and code quality. DI is a pattern that helps achieve those goals.

## How is DI implemented in Ichor?

Ichor provides two methods of using DI:

### Constructor Injection

Ichor contains some template deductions to figure out what interfaces the code requests from a constructor. This is the simplest form of using DI, but doesn't allow you to specify if the dependency is required/optional nor does it support multiple instances of the same type.

e.g. here is a service that adds a REST API endpoint to the running HTTP host service:

```c++
class BasicService final {
public:
    BasicService(IHttpHostService *hostService) {
        _routeRegistration = hostService->addRoute(HttpMethod::get, "/basic", [this, serializer](HttpRequest &req) -> AsyncGenerator<HttpResponse> {
            co_return HttpResponse{false, HttpStatus::ok, "application/text, "<html><body>This is my basic webpage</body></html>", {}};
        });
    }

private:
    std::unique_ptr<HttpRouteRegistration> _routeRegistration{};
};
```

Of course, Ichor needs to know about this type and this is done by registering it during initialization:

```c++
auto queue = std::make_unique<MultimapQueue>(spinlock);
auto &dm = queue->createManager();
dm.createServiceManager<HttpHostService, IHttpHostService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(80))}});
dm.createServiceManager<BasicService>();
queue->start(CaptureSigInt);
```

As soon as `HttpHostService` is initialized, Ichor notices that all requested dependencies of `BasicService` have been met and constructs it.

### Runtime Injection

The second option Ichor provides is to requests dependencies through a special constructor and allow the programmer to respond to a dependency becoming available or going away. This is especially useful when a piece of code is interested in multiple instances of the same type or if the dependency is optional. The [optional dependency example](../examples/optional_dependency_example) is a good show case of this.

The code generally follows this pattern:

```c++
class MyService final : public AdvancedService<TestService> {
public:
    // Creating instances of a service includes properties, and these need to be stored in the parent class. Be careful to move them each time and don't use the props variable but instead call getProperties(), if you need them.
    MyService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, true); // Request ILogger as a required dependency (the start function will only be called if all required dependencies have at least 1 instance available)
        reg.registerDependency<IOptionalService>(this, false); // Request IOptionalService as an optional dependency. This does not influence the start and stop functions.
    }
    ~MyService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        // this function is called when all required dependencies have been met with at least 1 instance
        co_return {};
    }

    Task<void> stop() final {
        // this function is called when the MyService instance is in a started state but one or more of the required dependencies are not available anymore. This function is guaranteed to be called before the removeDependencyInstance function is called for the dependency that is going away.
        co_return;
    }

    void addDependencyInstance(ILogger &, IService &) {
        // This function has this exact signature (so non-const reference parameters) and is called every time an ILogger instance is succesfully started.
    }

    void removeDependencyInstance(ILogger&, IService&) {
        // This function has this exact signature (so non-const reference parameters) and is called every time an ILogger instance is stopping.
    }

    void addDependencyInstance(IOptionalService&, IService &isvc) {
        // This function has this exact signature (so non-const reference parameters) and is called every time an IOptionalService instance is succesfully started.
    }

    void removeDependencyInstance(IOptionalService&, IService&) {
        // This function has this exact signature (so non-const reference parameters) and is called every time an IOptionalService instance is stopping.
    }

    // The dependency register needs to access the private functions to inject the dependencies
    // If you prefer, you can make the 6 functions above all public and then this won't be necessary. That's purely a stylistic choice, as the interface that other objects use won't have these functions mentioned at all.
    friend DependencyRegister;
};
```

## Extra Resources

[How to Use C++ Dependency Injection to Write Maintainable Software - Francesco Zoffoli CppCon 2022](https://www.youtube.com/watch?v=l6Y9PqyK1Mc)

[Cody Morterud's blog](https://www.codymorterud.com/design/2018/09/07/dependency-injection-cpp.html)

