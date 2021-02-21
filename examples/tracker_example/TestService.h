#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include "RuntimeCreatedService.h"

using namespace Ichor;

class TestService final : public Service<TestService> {
public:
    TestService(DependencyRegister &reg, IchorProperties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<IRuntimeCreatedService>(this, true, *getProperties());
    }
    ~TestService() final = default;
    bool start() final {
        ICHOR_LOG_INFO(_logger, "TestService started with dependency");
        getManager()->pushEvent<QuitEvent>(getServiceId());
        return true;
    }

    bool stop() final {
        ICHOR_LOG_INFO(_logger, "TestService stopped with dependency");
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;

        ICHOR_LOG_INFO(_logger, "Inserted logger svcid {} for svcid {}", logger->getServiceId(), getServiceId());
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

    void addDependencyInstance(IRuntimeCreatedService *svc) {
        auto ownScopeProp = getProperties()->find("scope");
        auto svcScopeProp = svc->getProperties()->find("scope");
        ICHOR_LOG_INFO(_logger, "Inserted IRuntimeCreatedService svcid {} with scope {} for svcid {} with scope {}", svc->getServiceId(), Ichor::any_cast<std::string&>(svcScopeProp->second), getServiceId(), Ichor::any_cast<std::string>(ownScopeProp->second));
    }

    void removeDependencyInstance(IRuntimeCreatedService *) {
    }

private:
    ILogger *_logger{nullptr};
};