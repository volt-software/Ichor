#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include "OptionalService.h"

using namespace Ichor;


struct ITestService : virtual public IService {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class TestService final : public ITestService, public Service {
public:
    TestService(DependencyRegister &reg, IchorProperties props) : Service(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<IOptionalService>(this, false);
    }
    ~TestService() final = default;
    bool start() final {
        LOG_INFO(_logger, "TestService started with dependency");
        _started = true;
        if(_injectionCount == 2) {
            getManager()->pushEvent<QuitEvent>(getServiceId());
        }
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

    void addDependencyInstance(IOptionalService *svc) {
        LOG_INFO(_logger, "Inserted IOptionalService svcid {}", svc->getServiceId());

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