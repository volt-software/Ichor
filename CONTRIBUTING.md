# Overview

Ichor is a C++20 framework for building microservices with an event-loop architecture, dependency injection, and strong emphasis on thread confinement and “fearless concurrency.” It aims to make multithreaded systems safer by confining each service to a particular event queue while still allowing services to communicate via events.

# Repository Structure

| Path | Purpose |
|------|---------|
| `include/` | Public headers. Key subfolders: `dependency_management` (service life‑cycle & injection), `event_queues`, `events`, `coroutines`, `stl` (custom utilities such as `RealtimeMutex`), and optional service interfaces under `services/`. |
| `src/` | Core implementations matching the headers above (`src/ichor`) plus optional service implementations under `src/services` (timer, networking, logging, etc.). |
| `examples/` | Small, focused demos. The minimal example shows the typical bootstrapping of an event loop, logger, and a service. Additional examples cover timers, HTTP/websocket, etc. |
| `docs/` | Guidance and design notes: getting started, dependency injection, CMake options, networking notes, and more. |
| `test/` | Unit tests (Catch2) demonstrating library usage. Useful as executable specifications. |
| `external/` | Vendored third‑party libraries (fmt, Catch2, spdlog, etc.). |

# Core Concepts

1. **Event Loops & Queues**
    - `IEventQueue` defines the interface for thread-local queues.
    - Events inherit from `Event`. They are inserted with `pushEvent`/`pushPrioritisedEvent`.
    - The queue consumes events by calling registered callbacks.

2. **Dependency Management**
    - `DependencyManager` orchestrates service lifecycles. Services implement `IService` (often via `AdvancedService` or `ConstructorInjectionService`).
    - It handles start/stop events, dependency tracking, and event callbacks/interceptors.

3. **Coroutines & Tasks**
    - Custom coroutine primitives (`Task`, `AsyncGenerator`, `AsyncManualResetEvent`) simplify asynchronous workflows without thread hazards.

4. **Custom STL Utilities**
    - Under `include/ichor/stl` are replacements like `RealtimeMutex` and `Any` to allow `-fno-rtti` builds and real-time-friendly primitives.

5. **Optional Services**
    - Pluggable services for timers, networking (Boost.Beast/TCP), logging (spdlog), metrics, Redis/Etcd integration, etc. Each lives under `include/ichor/services/<name>` and `src/services/<name>`.

6. **Configuration via CMake**
    - Many features are enabled/disabled through CMake options (e.g., `ICHOR_USE_SPDLOG`, `ICHOR_USE_BOOST_BEAST`, `ICHOR_USE_HARDENING`). See `docs/03-CMakeOptions.md`.

# Key Files to Explore

| File | Why it matters |
|------|----------------|
| `include/ichor/DependencyManager.h` | Heart of service management, event dispatch, and asynchronous event pushing. |
| `include/ichor/event_queues/IEventQueue.h` | Defines how event loops run and how events are queued. |
| `include/ichor/dependency_management/IService.h` | Basic service interface; all services derive from this. |
| `include/ichor/coroutines/Task.h` & `AsyncGenerator.h` | Coroutine primitives used throughout the framework. |
| `examples/minimal_example/main.cpp` | Most accessible introduction—shows how to spin up an event loop and service. |

# Getting Started

1. **Read the Docs**
    - Start with `docs/01-GettingStarted.md` for build/dependency instructions.
    - Continue with `docs/02-DependencyInjection.md` to understand the service model.
    - Review `docs/03-CMakeOptions.md` for feature toggles.

2. **Build & Run Examples**
    - Try `examples/minimal_example` to see the basic workflow.
    - Explore specialized examples (timer, networking, etc.) for advanced features.

3. **Review Tests**
    - Tests under `test/` show real-world use of the event system, dependency manager, and services. They’re compact and informative.

4. **Inspect Existing Services**
    - Look at `src/services/timer` or `src/services/network` to see how optional services are implemented and registered.

# Next Steps

- **Learn the event-driven model**: Understand how events are defined (`include/ichor/events`) and how callbacks/interceptors are registered in `DependencyManager`.
- **Dive into coroutines**: Explore `include/ichor/coroutines` to see how asynchronous tasks and generators integrate with the event loop.
- **Implement a new service**: Use `AdvancedService` or `ConstructorInjectionService` as a base and register it with the `DependencyManager`.
- **Experiment with configuration**: Toggle CMake options to enable logging or networking; note how services are compiled in conditionally.
- **Study concurrency approach**: Read about thread confinement and `RealtimeMutex` in `include/ichor/stl` for performance and safety insights.

This should give you a solid footing to begin exploring and extending Ichor. Happy hacking!
