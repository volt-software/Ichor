#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include "UselessService.h"

extern std::unique_ptr<Ichor::AsyncManualResetEvent> _evt;

namespace Ichor {
    struct IDependencyOnlineWhileStoppingService {
    };

    struct DependencyOnlineWhileStoppingService final : public IDependencyOnlineWhileStoppingService, public AdvancedService<DependencyOnlineWhileStoppingService> {
        DependencyOnlineWhileStoppingService(DependencyRegister &reg, Properties props, DependencyManager *mng) : AdvancedService(std::move(props), mng) {
            reg.registerDependency<IUselessService>(this, true);
        }
        ~DependencyOnlineWhileStoppingService() final = default;

        AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
            getManager().pushEvent<StopServiceEvent>(getServiceId(), svcId);
            co_return {};
        }

        AsyncGenerator<void> stop() final {
            getManager().pushEvent<StartServiceEvent>(getServiceId(), svcId);
            co_await *_evt;

            co_return;
        }

        void addDependencyInstance(IUselessService *, IService *svc) {
            svcId = svc->getServiceId();
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

        uint64_t svcId{};
    };
}
