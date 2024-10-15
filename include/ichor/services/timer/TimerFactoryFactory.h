#pragma once

#include <ichor/services/timer/ITimer.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/services/timer/IInternalTimerFactory.h>
#include <ichor/services/timer/ITimerTimerFactory.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/DependencyManager.h>

namespace Ichor {
    // Oh god, we're turning into java
    /// This class creates timer factories for requesting services, providing the requesting services' serviceId to the factory/timers
    class TimerFactoryFactory final : public ITimerTimerFactory, public AdvancedService<TimerFactoryFactory> {
    public:
        TimerFactoryFactory() = default;
        ~TimerFactoryFactory() final = default;

        std::vector<ServiceIdType> getCreatedTimerFactoryIds() const noexcept final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        AsyncGenerator<IchorBehaviour> handleDependencyRequest(AlwaysNull<ITimerFactory*>, DependencyRequestEvent const &evt);
        AsyncGenerator<IchorBehaviour> handleDependencyUndoRequest(AlwaysNull<ITimerFactory*>, DependencyUndoRequestEvent const &evt);

        friend DependencyManager;

        DependencyTrackerRegistration _trackerRegistration{};
        unordered_map<ServiceIdType, ServiceIdType> _factories;
        bool _quitting{};

        Task<void> pushStopEventForTimerFactory(ServiceIdType requestingSvcId, ServiceIdType factoryId) noexcept;
    };
}
