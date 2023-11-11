# ASIO supports io_uring, can do all of that, and more. Any reason i'd want to use your lib instead?

Ichor is a combination of things: Event Loops, Dependency Injection, as well as pre-made implementations for HTTP(s) servers/clients, websocket servers/clients, redis client and more.

Ichor is intended to be integrated on top of an existing event loop (e.g. [systemd](https://github.com/volt-software/Ichor/blob/main/src/ichor/event_queues/SdeventQueue.cpp) ). But if you have none, Ichor provides a simple implementation. Integrating Ichor into an existing boost.ASIO loop is possible, but not implemented today.

Boost.ASIO provides an event loop as well as channels, but doesn't give fine-grained control over which thread does what. This means having to think about thread safety inside handlers. See f.e. the [channels mutual exclusion](https://www.boost.org/doc/libs/1_83_0/doc/html/boost_asio/example/cpp20/channels/mutual_exclusion_1.cpp) example boost.ASIO provides.

Ichor forces one thread per 'executor', which reduces, but doesn't completely eliminate, needing to lock resources. Coroutines can still be put in a waiting state and therefore have shared state be mutated right under your nose.

Here's a short hand comparison between boost.ASIO and Ichor when it comes to event loops:

||Ichor|Boost.ASIO|
|:-|:-|:-|
|Integrate multiple event loops|✅|❌|
|Async File IO|Kinda, missing directory operations|Kinda, only on Windows (with I/O completion ports) and Linux (if using io_uring) and missing functionality such as deleting files and directory operations|
|io_uring|❌|✅ since Boost 1.78|
|Thread Pool|❌|✅|
|Threading|One thread per executor|one or more threads per executor|
|Cognitive Load Thread Safety|Low|Medium|

# But Boost.DI also provides DI. Why would I use your lib?

There are multiple functionalities that Boost.DI does not have but Ichor does: Optional Dependencies, Factories and Per-Instance Resolving being the more notable differences.

The best example here would be creating a new logger for each instance that is requesting a logger. That way, logger verbosity (or where/how to log each message) can be configured per instance. A use-case that I want Ichor to support is to be able to debug an up-and-running production executable. For example, a webserver that is giving HTTP 500 for logins. Using a logger per instance and exposing an admin control interface, the login related code's logger verbosity can be increased, without having to restart the executable and also without useless information.

Here's a short hand comparison between Ichor and various DI libraries:

||Ichor|Hypodermic|Boost.DI|Google.Fruit|CppMicroservices|
|:-|:-|:-|:-|:-|:-|
|Runtime/compile-time|Runtime|Runtime|Compile time (with some runtime)|Compile time (with some runtime)|Runtime|
|Constructor Injection|Yes|Yes|Yes|Yes|No|
|Per-instance resolving|Yes|No|No|No|Yes|
|Factories|Yes|No?|Partial|Partial|Yes|
|Coroutines|Yes|No|No|No|No|
|Optional dependencies|Yes|No|No|No|Yes?|
|Per-instance lifetime|Yes|No|No|No|Yes|
|Minimum c++|20|11|14|11|17|

# But Boost.BEAST also provides https and websockets. Why would I use your lib?

If you need all the bells and whistles that Boost.BEAST provides, then by all means, use that. If you need performance, the techempowered benchmarks will probably tell you to use Drogon or something similar. Ichor has a gap here.

||Ichor|Boost.BEAST|
|:-|:-|:-|
|Basic HTTP|✅|✅|
|Basic HTTPS|✅|✅|
|Basic Websocket|✅|✅|
|Basic ssl Websocket|❌|✅|
|Thread Pool|❌|✅|
|Threading|One thread per executor|one or more threads per executor|
|Cognitive Load Thread Safety|Low|Medium|

# Stop avoiding me and please answer the question, why would I use your lib?

The ultimate goal of Ichor is to reduce the cost of upgrading libraries or replacing them with others, without losing functionality. In the projects I've worked on, the code usually lasts multiple decades. Even simple decisions like using a logger become an expensive ordeal if the logger library is used directly (e.g. spdlog macro's all over your codebase). What if in a version upgrade the macro is changed? Or worse, remove? With Ichor's patterns, swapping loggers means refactoring only the logger implementation and nothing else. Everything using the loggers will never need to know about the change.

Obviously the caveat here is if the Ichor logger API ever needs a change, you probably still want to refactor everything. But Ichor then gives you the option to support partial refactors. Some classes continue to use the old API (but with a new logger implementation) and some other classes can already be refactored. Being able to choose where you need it the most and improve time to market immensely.

This benefit also applies to your own code, not just libraries. Detecting what sensor is connected and loading the right implementation at run time.

One of the more advanced use cases that I intend to support with Ichor is automatic code generation for API version differences. This is based on research using ComMA DSL code generation and open nets to model the differences, see Benny Akesson's paper [Pain-mitigation Techniques for Model-based Engineering using Domain-specific Languages](https://www.akesson.nl/files/pdf/akesson19-modcomp.pdf). ComMA does not support Ichor today, but one can dream :)
