#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/Service.h>
#include "ichor/dependency_management/ILifecycleManager.h"

using namespace Ichor;


struct IRuntimeCreatedService {
};

class RuntimeCreatedService final : public IRuntimeCreatedService, public Service<RuntimeCreatedService> {
public:
    RuntimeCreatedService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
    }

    ~RuntimeCreatedService() final = default;

private:
    AsyncGenerator<void> start() final {
        auto const& scope = Ichor::any_cast<std::string&>(_properties["scope"]);
        ICHOR_LOG_INFO(_logger, "RuntimeCreatedService started with scope {}", scope);
        co_return;
    }

    AsyncGenerator<void> stop() final {
        ICHOR_LOG_INFO(_logger, "RuntimeCreatedService stopped");
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