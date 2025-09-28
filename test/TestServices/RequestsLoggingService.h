#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/ServiceExecutionScope.h>

namespace Ichor {
    struct IRequestsLoggingService {
    };

    struct RequestsLoggingService final : public IRequestsLoggingService, public AdvancedService<RequestsLoggingService> {
        RequestsLoggingService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
            reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
        }

        ~RequestsLoggingService() = default;

        void addDependencyInstance(Ichor::ScopedServiceProxy<ILogger*>, IService&) {
        }

        void removeDependencyInstance(Ichor::ScopedServiceProxy<ILogger*>, IService&) {
        }
    };
}
