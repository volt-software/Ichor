#pragma once

#include <ichor/services/timer/ITimer.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/DependencyManager.h>

namespace Ichor {
    // Oh god, we're turning into java
    /// This class creates timer factories for requesting services, providing the requesting services' serviceId to the factory/timers
    class TimerFactoryFactory final : public AdvancedService<TimerFactoryFactory> {
    public:
        TimerFactoryFactory() = default;
        ~TimerFactoryFactory() final = default;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void handleDependencyRequest(AlwaysNull<ITimerFactory*>, DependencyRequestEvent const &evt);
        void handleDependencyUndoRequest(AlwaysNull<ITimerFactory*>, DependencyUndoRequestEvent const &evt);

        friend DependencyManager;

        DependencyTrackerRegistration _trackerRegistration{};
        unordered_map<uint64_t, IService*> _factories;
    };
}
