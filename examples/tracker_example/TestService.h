#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include "RuntimeCreatedService.h"

using namespace Ichor;

class TestService final : public Service<TestService> {
public:
    TestService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<IRuntimeCreatedService>(this, true, getProperties());
    }
    ~TestService() final = default;
    StartBehaviour start() final {
        ICHOR_LOG_INFO(_logger, "TestService started with dependency");
        getManager()->pushEvent<QuitEvent>(getServiceId());
        return StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        ICHOR_LOG_INFO(_logger, "TestService stopped with dependency");
        return StartBehaviour::SUCCEEDED;
    }

    void addDependencyInstance(ILogger *logger, IService *isvc) {
        _logger = logger;

        ICHOR_LOG_INFO(_logger, "Inserted logger svcid {} for svcid {}", isvc->getServiceId(), getServiceId());
    }

    void removeDependencyInstance(ILogger *logger, IService *isvc) {
        _logger = nullptr;
    }

    void addDependencyInstance(IRuntimeCreatedService *svc, IService *isvc) {
        auto const ownScopeProp = getProperties().find("scope");
        auto const svcScopeProp = isvc->getProperties().find("scope");
        ICHOR_LOG_INFO(_logger, "Inserted IRuntimeCreatedService svcid {} with scope {} for svcid {} with scope {}", isvc->getServiceId(), Ichor::any_cast<std::string&>(svcScopeProp->second), getServiceId(), Ichor::any_cast<std::string&>(ownScopeProp->second));
    }

    void removeDependencyInstance(IRuntimeCreatedService *, IService *isvc) {
    }

private:
    ILogger *_logger{nullptr};
};