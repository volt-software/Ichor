#pragma once

#include <ichor/Service.h>
#include <ichor/services/logging/Logger.h>

namespace Ichor {
    struct IRequestsLoggingService {
    };

    struct RequestsLoggingService final : public IRequestsLoggingService, public Service<RequestsLoggingService> {
        RequestsLoggingService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
            reg.registerDependency<ILogger>(this, true);
        }

        ~RequestsLoggingService() = default;

        void addDependencyInstance(ILogger *, IService *) {
        }

        void removeDependencyInstance(ILogger *, IService *) {
        }
    };
}