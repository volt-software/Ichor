# Factory Example

This example shows how to 'track' dependency requests. Specifically, this is most commonly used to create factories. 

When a service is created normally, that dependency is considered a globally available dependency. That is, every service requesting it, will get a handle to the very same dependency. In the case of loggers, it is usually desired to set the logging level on a more granular level, which can be solved by creating a specific logger for each individual request.




This demonstrates the following concepts:
* Fulfilling a dependency per-request, instead of globally
* How to use the `filter` feature to create a per-request dependency
