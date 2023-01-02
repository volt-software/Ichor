#pragma once

#include <ichor/dependency_management/Service.h>
#include "UselessService.h"

extern std::unique_ptr<Ichor::AsyncManualResetEvent> _evt;

namespace Ichor {
    struct IDependencyOfflineWhileStartingService {
    };

    struct DependencyOfflineWhileStartingService final : public IDependencyOfflineWhileStartingService, public Service<DependencyOfflineWhileStartingService> {
        DependencyOfflineWhileStartingService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
            reg.registerDependency<IUselessService>(this, true);
        }
        ~DependencyOfflineWhileStartingService() final = default;

        AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
            co_await *_evt;

            co_return {};
        }

        AsyncGenerator<void> stop() final {
            co_return;
        }

        void addDependencyInstance(IUselessService *, IService *svc) {
            getManager().pushEvent<StopServiceEvent>(getServiceId(), svc->getServiceId());
            if(getServiceState() != ServiceState::INSTALLED) {
                fmt::print("addDependencyInstance {}\n", getServiceState());
                std::terminate();
            }
        }

        void removeDependencyInstance(IUselessService *, IService *) {
            if(getServiceState() != ServiceState::INSTALLED) {
                fmt::print("removeDependencyInstance {}\n", getServiceState());
                std::terminate();
            }
        }
    };
}
