#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>

using namespace Ichor;


struct IRuntimeCreatedService : virtual public IService {
};

class RuntimeCreatedService final : public IRuntimeCreatedService, public Service {
public:
    RuntimeCreatedService(DependencyRegister &reg, IchorProperties props) : Service(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
    }

    ~RuntimeCreatedService() final = default;
    bool start() final {
        auto const& scope = std::any_cast<std::string&>(_properties["scope"]);
        LOG_INFO(_logger, "RuntimeCreatedService started with scope {}", scope);
        return true;
    }

    bool stop() final {
        LOG_INFO(_logger, "RuntimeCreatedService stopped");
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;
        LOG_INFO(_logger, "Inserted logger svcid {} for svcid {}", logger->getServiceId(), getServiceId());
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

private:
    ILogger *_logger{nullptr};
};