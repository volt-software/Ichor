#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/ILifecycleManager.h>

using namespace Ichor;


struct IOptionalService {
};

class OptionalService final : public IOptionalService, public AdvancedService<OptionalService> {
public:
    OptionalService(DependencyRegister &reg, Properties props, DependencyManager *mng) : AdvancedService(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
    }

    ~OptionalService() final = default;

private:
    AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "OptionalService {} started", getServiceId());
        co_return {};
    }

    AsyncGenerator<void> stop() final {
        ICHOR_LOG_INFO(_logger, "OptionalService {} stopped", getServiceId());
        co_return;
    }

    void addDependencyInstance(ILogger *logger, IService *isvc) {
        _logger = logger;
        ICHOR_LOG_INFO(_logger, "Inserted logger svcid {} for svcid {}", isvc->getServiceId(), getServiceId());
    }

    void removeDependencyInstance(ILogger *logger, IService *) {
        _logger = nullptr;
    }

    friend DependencyRegister;

    ILogger *_logger{nullptr};
};
