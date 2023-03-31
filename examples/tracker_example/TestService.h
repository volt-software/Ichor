#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/event_queues/IEventQueue.h>
#include "RuntimeCreatedService.h"

using namespace Ichor;

class TestService final : public AdvancedService<TestService> {
public:
    TestService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<IRuntimeCreatedService>(this, true, getProperties());
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

    void addDependencyInstance(ILogger &logger, IService &isvc) {
        _logger = &logger;

        ICHOR_LOG_INFO(_logger, "Inserted logger svcid {} for svcid {}", isvc.getServiceId(), getServiceId());
    }

    void removeDependencyInstance(ILogger&, IService &) {
        _logger = nullptr;
    }

    void addDependencyInstance(IRuntimeCreatedService &svc, IService &isvc) {
        auto const ownScopeProp = getProperties().find("scope");
        auto const svcScopeProp = isvc.getProperties().find("scope");
        ICHOR_LOG_INFO(_logger, "Inserted IRuntimeCreatedService svcid {} with scope {} for svcid {} with scope {}", isvc.getServiceId(), Ichor::any_cast<std::string&>(svcScopeProp->second), getServiceId(), Ichor::any_cast<std::string&>(ownScopeProp->second));
    }

    void removeDependencyInstance(IRuntimeCreatedService&, IService&) {
    }

    friend DependencyRegister;

    ILogger *_logger{};
};
