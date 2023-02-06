#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/logging/Logger.h>

namespace Ichor {
    struct IRequestsLoggingService {
    };

    struct RequestsLoggingService final : public IRequestsLoggingService, public AdvancedService<RequestsLoggingService> {
        RequestsLoggingService(DependencyRegister &reg, Properties props, DependencyManager *mng) : AdvancedService(std::move(props), mng) {
            reg.registerDependency<ILogger>(this, true);
        }

        ~RequestsLoggingService() = default;

        void addDependencyInstance(ILogger *, IService *) {
        }

        void removeDependencyInstance(ILogger *, IService *) {
        }
    };
}
