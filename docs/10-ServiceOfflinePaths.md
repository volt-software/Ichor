# Service Offline Paths

This document describes the main ways a service transitions to offline in Ichor and the event chains involved. It focuses on the DependencyManager event flow and how asynchronous lifecycles can delay a stop.

## Glossary

- Service states: INSTALLED -> STARTING -> INJECTING -> ACTIVE -> UNINJECTING -> STOPPING -> INSTALLED
- StopServiceEvent: request to stop a service.
- DependencyOfflineEvent: uninject a service from its dependees before stopping it.
- ContinuableDependencyOfflineEvent: internal event that resumes an async dependencyOffline coroutine.
- ContinuableEvent: internal event that resumes any async coroutine (start/stop/dependency handling).
- DependencyRequestEvent / DependencyUndoRequestEvent: used by factory-style services to create and tear down dependencies on demand.

## Path 1: Direct stop (user or framework enqueues StopServiceEvent)

```mermaid
flowchart TD
    A[Caller pushes StopServiceEvent for S] --> B[DependencyManager handles StopServiceEvent]
    B --> C{S already INSTALLED or STOPPING?}
    C -->|yes| D[Ignore StopServiceEvent]
    C -->|no| E{existing coroutine for S?}
    E -->|yes| F[Record pending stop; wait for ContinuableEvent]
    E -->|no| G{dependees empty and S is UNINJECTING?}
    G -->|no| H[Push DependencyOfflineEvent for S]
    H --> I[Each dependee runs dependencyOffline]
    I --> J{dependencyOffline async?}
    J -->|yes| K[ContinuableDependencyOfflineEvent + ContinuableEvent]
    J -->|no| L[finishDependencyOffline now]
    K --> L
    L --> M{dependees empty now?}
    M -->|yes| N[Requeue StopServiceEvent for S]
    M -->|no| O[Wait for more dependees]
    G -->|yes| P[Run S.stop coroutine]
    P --> Q{stop async?}
    Q -->|yes| R[ContinuableEvent resumes stop]
    Q -->|no| S[Set INSTALLED, clear registrations, finish waiters, removeAfter]
    R --> S
```

Key notes:
- If any coroutine for S is still running, StopServiceEvent is deferred and retried after a ContinuableEvent completes.
- DependencyOfflineEvent is used to remove S from dependees before S.stop().

## Path 2: Dependency goes away (required dependency stops)

Example: A depends on B (required). B is stopped by any cause (direct stop or quit). A must uninject and may stop before B finishes.

```mermaid
flowchart TD
    A1[StopServiceEvent for B] --> B1[DependencyOfflineEvent for B]
    B1 --> C1[DependencyManager iterates dependees of B]
    C1 --> D1[A.dependencyOffline B]
    D1 --> E1{A decides to stop?}
    E1 -->|no| F1[finishDependencyOffline A, B]
    E1 -->|yes| G1[A pushes StopServiceEvent for A and waits]
    G1 --> H1[StopServiceEvent for A runs stop]
    H1 --> I1[A reaches INSTALLED]
    I1 --> J1[B sees dependees empty]
    J1 --> K1[StopServiceEvent for B runs stop]

    D1 --> M1{A dependencyOffline async?}
    M1 -->|yes| N1[ContinuableDependencyOfflineEvent for A]
    N1 --> O1[ContinuableEvent resumes A]
    O1 --> E1
```

Key notes:
- The dependency manager keeps B in UNINJECTING until all dependees have finished their dependencyOffline handling.
- If A uses async dependencyOffline, B waits on the ContinuableDependencyOfflineEvent path.
- If A decides to stop, A is stopped first; only after A is fully stopped does B proceed to stop.

## Path 3: Factory-driven teardown (dependency no longer required)

Factory services often create per-requesting-service instances (e.g. TimerFactoryFactory). When the requesting service is removed or its dependency is undone, the factory stops the created service.

```mermaid
flowchart TD
    A2[Requesting service R removed or no longer needs IFace] --> B2[DependencyUndoRequestEvent for IFace]
    B2 --> C2[Factory handles DependencyUndoRequestEvent]
    C2 --> D2[Factory stops created service F]
    D2 --> E2[Push StopServiceEvent for F removeAfter = true]
    E2 --> F2[DependencyManager runs standard stop path]
    F2 --> G2[DependencyOfflineEvent for F]
    G2 --> H2[Dependees of F uninject]
    H2 --> I2[StopServiceEvent for F -> F.stop]
    I2 --> J2[F removed from registry]
```

Key notes:
- The factory path uses the same stop machinery as a direct stop, but the initiator is the factory reacting to DependencyUndoRequestEvent.
- Factories may perform async cleanup (e.g. stop timers) before pushing StopServiceEvent.

## Path 4: QuitEvent cascade (process shutdown)

QuitEvent triggers StopServiceEvent for all active services and their dependees. The same stop and dependencyOffline chain is used, but the originator is the manager.

```mermaid
flowchart TD
    Q1[QuitEvent] --> Q2[DependencyManager enumerates services]
    Q2 --> Q3[Push StopServiceEvent for all active services]
    Q3 --> Q4[Each service follows Path 1]
```

## Async handling and delayed stops

- Any async start/stop/dependencyOffline coroutine is stored in scoped generators and resumed by ContinuableEvent.
- StopServiceEvent can be delayed if an outstanding coroutine is tied to the same service; the manager defers the stop until the coroutine completes.

## References

- Event handling: src/ichor/DependencyManager.cpp
- dependencyOffline logic: include/ichor/dependency_management/DependencyLifecycleManager.h
- Continuable events: include/ichor/events/ContinuableEvent.h
- Factory example: src/services/timer/TimerFactoryFactory.cpp
