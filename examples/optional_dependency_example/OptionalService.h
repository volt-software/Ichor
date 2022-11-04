#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>

using namespace Ichor;


struct IOptionalService {
};

class OptionalService final : public IOptionalService, public Service<OptionalService> {
public:
    OptionalService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
    }

    ~OptionalService() final = default;

private:
    StartBehaviour start() final {
        ICHOR_LOG_INFO(_logger, "OptionalService {} started", getServiceId());
        return StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        ICHOR_LOG_INFO(_logger, "OptionalService {} stopped", getServiceId());
        return StartBehaviour::SUCCEEDED;
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