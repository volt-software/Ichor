#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/Service.h>
#include <ichor/dependency_management/ILifecycleManager.h>

using namespace Ichor;


struct ITestService {
};

class TestService final : public ITestService, public Service<TestService> {
public:
    TestService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~TestService() final = default;

private:
    AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
        co_return {};
    }

    AsyncGenerator<void> stop() final {
        co_return;
    }

    void addDependencyInstance(ILogger *logger, IService *) {
        _logger = logger;
    }

    void removeDependencyInstance(ILogger *logger, IService *) {
        _logger = nullptr;
    }

    friend DependencyRegister;

    ILogger *_logger;
};
