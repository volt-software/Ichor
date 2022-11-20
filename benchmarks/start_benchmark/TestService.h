#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/Service.h>
#include "ichor/dependency_management/ILifecycleManager.h"

#ifdef __SANITIZE_ADDRESS__
constexpr uint32_t SERVICES_COUNT = 1'000;
#else
constexpr uint32_t SERVICES_COUNT = 10'000;
#endif

using namespace Ichor;

class TestService final : public Service<TestService> {
public:
    TestService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~TestService() final = default;

private:
    AsyncGenerator<void> start() final {
        auto iteration = Ichor::any_cast<uint64_t>(getProperties()["Iteration"]);
        if(iteration == SERVICES_COUNT - 1) {
            getManager().pushEvent<QuitEvent>(getServiceId());
        }
        co_return;
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

    ILogger *_logger{nullptr};
};