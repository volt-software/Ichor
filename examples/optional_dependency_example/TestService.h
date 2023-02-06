#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/ILifecycleManager.h>
#include "OptionalService.h"

using namespace Ichor;

class TestService final : public AdvancedService<TestService> {
public:
    TestService(DependencyRegister &reg, Properties props, DependencyManager *mng) : AdvancedService(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<IOptionalService>(this, false);
    }
    ~TestService() final = default;

private:
    AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "TestService started with dependency");
        _started = true;
        if(_injectionCount == 2) {
            getManager().pushEvent<QuitEvent>(getServiceId());
        }
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

    void removeDependencyInstance(ILogger *logger, IService *) {
        _logger = nullptr;
    }

    void addDependencyInstance(IOptionalService *svc, IService *isvc) {
        ICHOR_LOG_INFO(_logger, "Inserted IOptionalService svcid {}", isvc->getServiceId());

        _injectionCount++;
        if(_started && _injectionCount == 2) {
            getManager().pushEvent<QuitEvent>(getServiceId());
        }
    }

    void removeDependencyInstance(IOptionalService *, IService *) {
    }

    friend DependencyRegister;

    ILogger *_logger{nullptr};
    bool _started{false};
    int _injectionCount{0};
};
