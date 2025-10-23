#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include "UselessService.h"
#include <ichor/ServiceExecutionScope.h>

extern std::unique_ptr<Ichor::AsyncManualResetEvent> _evt;

namespace Ichor {
    struct IDependencyOnlineWhileStoppingService {
    };

    struct DependencyOnlineWhileStoppingService final : public IDependencyOnlineWhileStoppingService, public AdvancedService<DependencyOnlineWhileStoppingService> {
        DependencyOnlineWhileStoppingService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
            reg.registerDependency<IUselessService>(this, DependencyFlags::REQUIRED);
        }
        ~DependencyOnlineWhileStoppingService() final = default;

        Task<tl::expected<void, Ichor::StartError>> start() final {
            if(!cycled) {
                GetThreadLocalEventQueue().pushPrioritisedEvent<StopServiceEvent>(getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, svcId);
            }
            co_return {};
        }

        Task<void> stop() final {
            GetThreadLocalEventQueue().pushPrioritisedEvent<StartServiceEvent>(getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, svcId);
            co_await *_evt;
            cycled = true;

            co_return;
        }

        void addDependencyInstance(Ichor::ScopedServiceProxy<IUselessService*>, IService &svc) {
            svcId = svc.getServiceId();
            if(getServiceState() != ServiceState::INSTALLED) {
                fmt::print("addDependencyInstance {}\n", getServiceState());
                std::terminate();
            }
        }

        void removeDependencyInstance(Ichor::ScopedServiceProxy<IUselessService*>, IService&) {
            if(getServiceState() != ServiceState::INSTALLED) {
                fmt::print("removeDependencyInstance {}\n", getServiceState());
                std::terminate();
            }
        }

        ServiceIdType svcId{};
        bool cycled{};
    };
}
