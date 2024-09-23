#pragma once

#ifndef ICHOR_USE_LIBURING
#error "Ichor has not been compiled with io_uring support"
#endif

#include <ichor/services/timer/ITimer.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/services/timer/IInternalTimerFactory.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/event_queues/IIOUringQueue.h>
#include <ichor/DependencyManager.h>

namespace Ichor {
    /// This class creates timer factories for requesting services, providing the requesting services' serviceId to the factory/timers
    class IOUringTimerFactoryFactory final : public AdvancedService<IOUringTimerFactoryFactory> {
    public:
        IOUringTimerFactoryFactory(DependencyRegister &reg, Properties props);
        ~IOUringTimerFactoryFactory() final = default;

        void addDependencyInstance(IIOUringQueue &, IService&) noexcept;
        void removeDependencyInstance(IIOUringQueue &, IService&) noexcept;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void handleDependencyRequest(AlwaysNull<ITimerFactory*>, DependencyRequestEvent const &evt);
        void handleDependencyUndoRequest(AlwaysNull<ITimerFactory*>, DependencyUndoRequestEvent const &evt);

        friend DependencyManager;

        IIOUringQueue *_q{};
        DependencyTrackerRegistration _trackerRegistration{};
        unordered_map<ServiceIdType, ServiceIdType> _factories;
        AsyncManualResetEvent _quitEvt;

        decltype(_factories)::iterator pushStopEventForTimerFactory(decltype(_factories)::iterator const &factory) noexcept;
    };
}
