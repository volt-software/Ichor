#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/event_queues/IEventQueue.h>
#include "RuntimeCreatedService.h"
#include <ichor/ServiceExecutionScope.h>

using namespace Ichor;
using namespace Ichor::v1;

class TestService final : public AdvancedService<TestService> {
public:
    TestService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
        reg.registerDependency<IRuntimeCreatedService>(this, DependencyFlags::REQUIRED, getProperties());
    }
    ~TestService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "TestService started with dependency");
        GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());
        co_return {};
    }

    Task<void> stop() final {
        ICHOR_LOG_INFO(_logger, "TestService stopped with dependency");
        co_return;
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService &isvc) {
        _logger = std::move(logger);

        ICHOR_LOG_INFO(_logger, "Inserted logger svcid {} for svcid {}", isvc.getServiceId(), getServiceId());
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<ILogger*>, IService &) {
        _logger.reset();
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<IRuntimeCreatedService*> svc, IService &isvc) {
        auto const ownScopeProp = getProperties().find("scope");
        auto const svcScopeProp = isvc.getProperties().find("scope");
        ICHOR_LOG_INFO(_logger, "Inserted IRuntimeCreatedService svcid {} with scope {} for svcid {} with scope {}", isvc.getServiceId(), Ichor::v1::any_cast<std::string&>(svcScopeProp->second), getServiceId(), Ichor::v1::any_cast<std::string&>(ownScopeProp->second));
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<IRuntimeCreatedService*>, IService&) {
    }

    friend DependencyRegister;

    Ichor::ScopedServiceProxy<ILogger*> _logger {};
};
