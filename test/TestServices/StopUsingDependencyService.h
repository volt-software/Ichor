#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include "UselessService.h"
#include <ichor/ScopedServiceProxy.h>

namespace Ichor {
    /// Stops itself, and its required dependency goes offline while its stop() is suspended. Its stop() still uses that dependency afterwards.
    struct StopUsingDependencyService final : public AdvancedService<StopUsingDependencyService> {
        StopUsingDependencyService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
            reg.registerDependency<IUselessService>(this, DependencyFlags::REQUIRED);
        }
        ~StopUsingDependencyService() final = default;

        Task<tl::expected<void, Ichor::StartError>> start() final {
            GetThreadLocalEventQueue().pushPrioritisedEvent<StopServiceEvent>(getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, getServiceId(), true);
            co_return {};
        }

        Task<void> stop() final {
            // The dependency goes offline while this is suspended, the resume comes after that has been handled
            GetThreadLocalEventQueue().pushPrioritisedEvent<StopServiceEvent>(getServiceId(), 50, _uselessServiceId, true);
            GetThreadLocalEventQueue().pushPrioritisedEvent<RunFunctionEvent>(getServiceId(), 10'000, [this]() {
                _evt.set();
            });
            co_await _evt;
            dependencyPresentAfterSuspending = _useless != nullptr;
            GetThreadLocalEventQueue().pushPrioritisedEvent<QuitEvent>(getServiceId(), 20'000);
            co_return;
        }

        void addDependencyInstance(Ichor::ScopedServiceProxy<IUselessService*> useless, IService &isvc) {
            _useless = std::move(useless);
            _uselessServiceId = isvc.getServiceId();
        }

        void removeDependencyInstance(Ichor::ScopedServiceProxy<IUselessService*>, IService&) {
            removedWhileStopping = getServiceState() == ServiceState::STOPPING;
            _useless = nullptr;
        }

        static inline bool dependencyPresentAfterSuspending{};
        static inline bool removedWhileStopping{};

        Ichor::ScopedServiceProxy<IUselessService*> _useless{};
        ServiceIdType _uselessServiceId{};
        Ichor::AsyncManualResetEvent _evt;
    };
}
