#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include "OptionalService.h"

using namespace Ichor;

class TestService final : public Service<TestService> {
public:
    TestService(DependencyRegister &reg, IchorProperties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<IOptionalService>(this, false);
    }
    ~TestService() final = default;
    bool start() final {
        ICHOR_LOG_INFO(_logger, "TestService started with dependency");
        _started = true;
        if(_injectionCount == 2) {
            getManager()->pushEvent<QuitEvent>(getServiceId());
        }
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

    void addDependencyInstance(IOptionalService *svc) {
        ICHOR_LOG_INFO(_logger, "Inserted IOptionalService svcid {}", svc->getServiceId());

        _injectionCount++;
        if(_started && _injectionCount == 2) {
            getManager()->pushEvent<QuitEvent>(getServiceId());
        }
    }

    void removeDependencyInstance(IOptionalService *) {
    }

private:
    ILogger *_logger{nullptr};
    bool _started{false};
    int _injectionCount{0};
};