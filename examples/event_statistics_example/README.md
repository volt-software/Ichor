# Event Statistics Example

This example showcases Ichor's capability to hook into the event loop system. It uses the pre- and post-event hooks to determine how long each event took to resolve and prints out the min/max/avg per event type at exit.

It uses the `EventStatisticsService` provided by ichor, which in turn uses `preInterceptEvent` and `postInterceptEvent`.

This demonstrates the following concepts:
* Using the provided `EventStatisticsService` for performance measurements
* Using the Timer subsystem to generate some artificial load to measure
* How to implement ones own Event Interceptor service (see below)

Implementing such a service yourself is also possible by calling the `registerEventInterceptor<>` function on the Dependency Manager:

```c++
class MyClass {
public:
    MyClass(DependencyManager *dm) {
        // register the interceptor and keep a reference it, as the destructor automatically unregisters it.
        _interceptorRegistration = dm->registerEventInterceptor<Event>(this, this);
    }
    
    void preInterceptEvent(Event const &evt) {
        // We don't need to check for evt.id or evt.type here, as postInterceptEvent() is guaranteed to be called directly after processing the event
        // The downside is then that each async execution point of an event is counted as a separate instance. I.e. every co_await is a new event with its own processing time.
        _startProcessingTimestamp = std::chrono::steady_clock::now();
    }
    
    void postInterceptEvent(Event const &evt, bool processed) {
        auto now = std::chrono::steady_clock::now();
        auto processingTime = now - _startProcessingTimestamp;
        fmt::print("Processing of event type {} took {} ns", evt.name, std::chrono::duration_cast<std::chrono::nanoseconds>(processingTime).count());
    }
    
private:
    EventInterceptorRegistration _interceptorRegistration{};
    std::chrono::time_point<std::chrono::steady_clock> _startProcessingTimestamp{};
};
```
