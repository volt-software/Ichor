# Sharing State

Programs would not be worth much unless they also modify some state. However, shared state also means having to think about access patterns. There are a couple of factors that impact the decision on how to protect the state:

* Concurrency
* Interrupts
* Coroutines

As Ichor adds coroutines to the mix, we might encounter situations where the standard approaches are not enough. Let's go over them:

## Existing approaches in today's programs

### Single Threaded, no coroutines/interrupts

If your program only contains a single thread and you have shared state, but are not using coroutines and interrupts, then you're off easy. No protection is needed.

### Multi Threaded, no coroutines/interrupts

The other common pattern out there in programs today is where there are multiple threads accessing the same data. This is usually solved using mutexes or thread-safe data structures or using concurrent queues.  

## What patterns Ichor offers

### Bog-standard `std::mutex`

It is definitely possible to use a `std::mutex` in combination with Ichor. If the mutex is just guarding some data without needing to do I/O, this will work fine and is unlikely to cause bottlenecks. However, if your code is waiting on I/O to complete, the entire thread is blocked until the mutex stops blocking. This can result in 

### Ownership per thread combined with queues, a.k.a. Rust's Channels

Ichor prefers this approach, where data is not shared between threads (optionally shared within the same thread). If another thread needs to read of write to data, an event should be placed into the other thread's Ichor::Queue and resolved there. That way, the only point where concurrent reads/writes occur are in Ichor's event queue, which conveniently is thread-safe.

However, what happens if one needs state, residing in the same thread, to be consistent accross coroutines? For that, Ichor offers an `AsyncSingleThreadedMutex`:

```c++
struct MySharedData {
    // your important data
    int completed_io_operations{};
};
struct IMyService {
    virtual ~IMyService() = default;
    virtual Task<void> my_async_call() = 0;
};
class MyService final : public IMyService {
public:
    Task<void> my_async_call() final {
        AsyncSingleThreadedLockGuard lg = co_await _m.lock(); // unliked std::mutex, this call does not block the thread.
        // ... co_await some I/O while still holding the lock ...
        _data.completed_io_operations++;
        co_return {};
        // async mutex unlocks when lg goes out of scope
    }
private:
    AsyncSingleThreadedMutex _m;
    MySharedData _data;
};
```

The benefit of the `AsyncSingleThreadedMutex` is that unlike `std::mutex`, it does not block the thread. Instead, whenever the `AsyncSingleThreadedMutex` is locked, it queues up the function wanting to lock it and continues running that function only when the mutex is unlocked.
