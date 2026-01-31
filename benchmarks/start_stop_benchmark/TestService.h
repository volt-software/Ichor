#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/ScopedServiceProxy.h>

using namespace Ichor;
using namespace Ichor::v1;


struct ITestService {
};

class TestService final : public ITestService, public AdvancedService<TestService> {
public:
    TestService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
    }
    ~TestService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        co_return {};
    }

    Task<void> stop() final {
        co_return;
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService &) {
        _logger = std::move(logger);
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService&) {
        _logger.reset();
    }

    friend DependencyRegister;

    Ichor::ScopedServiceProxy<ILogger*> _logger ;
};
