#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/ScopedServiceProxy.h>

using namespace Ichor;
using namespace Ichor::v1;


struct IOptionalService {
};

class OptionalService final : public IOptionalService, public AdvancedService<OptionalService> {
public:
    OptionalService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
    }

    ~OptionalService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "OptionalService {} started", getServiceId());
        co_return {};
    }

    Task<void> stop() final {
        ICHOR_LOG_INFO(_logger, "OptionalService {} stopped", getServiceId());
        co_return;
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService &isvc) {
        _logger = std::move(logger);
        ICHOR_LOG_INFO(_logger, "Inserted logger svcid {} for svcid {}", isvc.getServiceId(), getServiceId());
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<ILogger*>, IService&) {
        _logger.reset();
    }

    friend DependencyRegister;

    Ichor::ScopedServiceProxy<ILogger*> _logger {};
};
