#pragma once

#include <framework/DependencyManager.h>
#include <optional_bundles/logging_bundle/Logger.h>
#include "framework/Service.h"
#include "framework/LifecycleManager.h"
#include "RuntimeCreatedService.h"

using namespace Cppelix;


struct ITestService : virtual public IService {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class TestService final : public ITestService, public Service {
public:
    ~TestService() final = default;
    bool start() final {
        LOG_INFO(_logger, "TestService started with dependency");
        getManager()->pushEvent<QuitEvent>(getServiceId());
        return true;
    }

    bool stop() final {
        LOG_INFO(_logger, "TestService stopped with dependency");
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;

        LOG_INFO(_logger, "Inserted logger svcid {} for svcid {}", logger->getServiceId(), getServiceId());
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

    void addDependencyInstance(IRuntimeCreatedService *svc) {
        auto ownScopeProp = getProperties()->find("scope");
        auto svcScopeProp = svc->getProperties()->find("scope");
        LOG_INFO(_logger, "Inserted IRuntimeCreatedService svcid {} with scope {} for svcid {} with scope {}", svc->getServiceId(), std::any_cast<std::string>(svcScopeProp->second), getServiceId(), std::any_cast<std::string>(ownScopeProp->second));
    }

    void removeDependencyInstance(IRuntimeCreatedService *) {
    }

private:
    ILogger *_logger;
};