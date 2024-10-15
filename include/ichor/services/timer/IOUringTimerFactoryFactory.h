#pragma once

#ifndef ICHOR_USE_LIBURING
#error "Ichor has not been compiled with io_uring support"
#endif

#include <ichor/services/timer/ITimer.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/services/timer/IInternalTimerFactory.h>
#include <ichor/services/timer/ITimerTimerFactory.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/event_queues/IIOUringQueue.h>
#include <ichor/DependencyManager.h>

namespace Ichor {
    /// This class creates timer factories for requesting services, providing the requesting services' serviceId to the factory/timers
    class IOUringTimerFactoryFactory final :  public ITimerTimerFactory, public AdvancedService<IOUringTimerFactoryFactory> {
    public:
        IOUringTimerFactoryFactory(DependencyRegister &reg, Properties props);
        ~IOUringTimerFactoryFactory() final = default;

        void addDependencyInstance(IIOUringQueue &, IService&) noexcept;
        void removeDependencyInstance(IIOUringQueue &, IService&) noexcept;

        std::vector<ServiceIdType> getCreatedTimerFactoryIds() const noexcept final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        AsyncGenerator<IchorBehaviour> handleDependencyRequest(AlwaysNull<ITimerFactory*>, DependencyRequestEvent const &evt);
        AsyncGenerator<IchorBehaviour> handleDependencyUndoRequest(AlwaysNull<ITimerFactory*>, DependencyUndoRequestEvent const &evt);

        friend DependencyManager;

        IIOUringQueue *_q{};
        DependencyTrackerRegistration _trackerRegistration{};
        unordered_map<ServiceIdType, ServiceIdType> _factories;
        bool _quitting{};

        Task<void> pushStopEventForTimerFactory(ServiceIdType requestingSvcId, ServiceIdType factoryId) noexcept;
    };
}
