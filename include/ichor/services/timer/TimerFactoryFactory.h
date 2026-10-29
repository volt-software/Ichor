#pragma once

#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/services/timer/ITimerTimerFactory.h>
#include <ichor/services/timer/TimerFactoryTracker.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/event_queues/IEventQueue.h>
#include <ichor/DependencyManager.h>

namespace Ichor::v1 {
    class Timer;

    // Oh god, we're turning into java
    /// This class creates timer factories for requesting services, providing the requesting services' serviceId to the factory/timers
    class TimerFactoryFactory final : public ITimerTimerFactory, public AdvancedService<TimerFactoryFactory> {
    public:
        TimerFactoryFactory() = default;
        ~TimerFactoryFactory() final = default;

        [[nodiscard]] std::vector<ServiceIdType> getCreatedTimerFactoryIds() const noexcept final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        AsyncGenerator<IchorBehaviour> handleDependencyRequest(v1::AlwaysNull<ITimerFactory*>, DependencyRequestEvent const &evt);
        AsyncGenerator<IchorBehaviour> handleDependencyUndoRequest(v1::AlwaysNull<ITimerFactory*>, DependencyUndoRequestEvent const &evt);

        friend DependencyManager;

        DependencyTrackerRegistration _trackerRegistration{};
        Detail::TimerFactoryTracker<Timer, IEventQueue> _tracker;
    };
}
