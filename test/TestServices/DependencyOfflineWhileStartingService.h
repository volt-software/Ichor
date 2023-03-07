#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include "UselessService.h"

extern std::unique_ptr<Ichor::AsyncManualResetEvent> _evt;

namespace Ichor {
    struct IDependencyOfflineWhileStartingService {
    };

    struct DependencyOfflineWhileStartingService final : public IDependencyOfflineWhileStartingService, public AdvancedService<DependencyOfflineWhileStartingService> {
        DependencyOfflineWhileStartingService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
            reg.registerDependency<IUselessService>(this, true);
        }
        ~DependencyOfflineWhileStartingService() final = default;

        Task<tl::expected<void, Ichor::StartError>> start() final {
            co_await *_evt;

            co_return {};
        }

        Task<void> stop() final {
            co_return;
        }

        void addDependencyInstance(IUselessService&, IService &svc) {
            GetThreadLocalEventQueue().pushEvent<StopServiceEvent>(getServiceId(), svc.getServiceId());
            if(getServiceState() != ServiceState::INSTALLED) {
                fmt::print("addDependencyInstance {}\n", getServiceState());
                std::terminate();
            }
        }

        void removeDependencyInstance(IUselessService&, IService&) {
            if(getServiceState() != ServiceState::INSTALLED) {
                fmt::print("removeDependencyInstance {}\n", getServiceState());
                std::terminate();
            }
        }
    };
}
