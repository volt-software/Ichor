#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/ILifecycleManager.h>
#include "RuntimeCreatedService.h"

using namespace Ichor;

class TestService final : public AdvancedService<TestService> {
public:
    TestService(DependencyRegister &reg, Properties props, DependencyManager *mng) : AdvancedService(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<IRuntimeCreatedService>(this, true, getProperties());
    }
    ~TestService() final = default;

private:
    AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "TestService started with dependency");
        getManager().pushEvent<QuitEvent>(getServiceId());
        co_return {};
    }

    AsyncGenerator<void> stop() final {
        ICHOR_LOG_INFO(_logger, "TestService stopped with dependency");
        co_return;
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

    friend DependencyRegister;

    ILogger *_logger{nullptr};
};
