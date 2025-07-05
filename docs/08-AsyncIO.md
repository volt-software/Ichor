# Async I/O

Although early access, Ichor offers a couple of async file I/O operations on Linux, Windows and OS X:

* Reading files
* Copying files
* Removing files and
* Writing files

These operations can take a relatively long time. As a quick reminder, [these are the latency numbers everyone should know](https://gist.github.com/jboner/2841832).
The abstraction Ichor provides allows you to queue up file I/O on another thread without blocking the current. 

## Example

Here's an example showcasing reading and writing a file:

```c++
auto queue = std::make_unique<PriorityQueue>();
auto &dm = queue->createManager();
uint64_t ioSvcId{};

// set up and run the Ichor Thread
std::thread t([&]() {
    ioSvcId = dm.createServiceManager<SharedOverThreadsAsyncFileIO, IAsyncFileIO>()->getServiceId();
    queue->start(CaptureSigInt);
});

// Create a file the "old" way to ensure it exists for this example
{
    std::ofstream out("AsyncFileIO.txt");
    out << "This is a test";
    out.close();
}

// Simulate running an important task manually for this example. Normally one would use dependency injection.
queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
    // get a handle to the AsyncFileIO implementation
    auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);

    // enqueue reading the file on another thread and co_await its result
    // not using auto to show the type in example. Using auto would be a lot easier here.
    tl::expected<std::string, Ichor::v1::IOError> ret = co_await async_io_svc->first->readWholeFile("AsyncFileIO.txt");

    if(!ret || ret != "This is a test") {
        fmt::print("Couldn't read file\n");
        co_return {};
    }

    // enqueue writing to file (automatically overwrites if already exists)
    tl::expected<void, Ichor::v1::IOError> ret2 = co_await async_io_svc->first->writeFile("AsyncFileIO.txt", "Overwrite");

    if(!ret2) {
        fmt::print("Couldn't write file\n");
        co_return {};
    }

    ret = co_await async_io_svc->first->readWholeFile("AsyncFileIO.txt");

    if(!ret || ret != "Overwrite") {
        fmt::print("Couldn't read file\n");
        co_return {};
    }

    // Quit Ichor thread
    queue->pushEvent<QuitEvent>(0);
    co_return {};
});

t.join();
```

## Possible different implementations

Ichor currently provides only two types of implementation due to time constraints: one I/O thread shared over all Ichor threads and a linux-only io_uring implementation. Obviously, this won't fit all use cases. Creating a new implementation is possible that, for example, uses multiple I/O threads and schedules requests round robin. As long as the implementation adheres to the `IAsyncFileIO` interface, it is possible to swap.
