#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>

using namespace Ichor;


struct IOptionalService : virtual public IService {
};

class OptionalService final : public IOptionalService, public Service<OptionalService> {
public:
    OptionalService(DependencyRegister &reg, IchorProperties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
    }

    ~OptionalService() final = default;
    bool start() final {
        ICHOR_LOG_INFO(_logger, "OptionalService {} started", getServiceId());
        return true;
    }

    bool stop() final {
        ICHOR_LOG_INFO(_logger, "OptionalService {} stopped", getServiceId());
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;
        ICHOR_LOG_INFO(_logger, "Inserted logger svcid {} for svcid {}", logger->getServiceId(), getServiceId());
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

private:
    ILogger *_logger{nullptr};
};