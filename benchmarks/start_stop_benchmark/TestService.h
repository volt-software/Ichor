#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>

using namespace Ichor;


struct ITestService {
};

class TestService final : public ITestService, public Service<TestService> {
public:
    TestService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~TestService() final = default;
    StartBehaviour start() final {
        return Ichor::StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        return Ichor::StartBehaviour::SUCCEEDED;
    }

    void addDependencyInstance(ILogger *logger, IService *) {
        _logger = logger;
    }

    void removeDependencyInstance(ILogger *logger, IService *) {
        _logger = nullptr;
    }

private:
    ILogger *_logger;
};