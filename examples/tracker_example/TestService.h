#pragma once

#include <spdlog/spdlog.h>
#include <framework/DependencyManager.h>
#include <framework/interfaces/IFrameworkLogger.h>
#include "framework/Service.h"
#include "framework/Framework.h"
#include "framework/ServiceLifecycleManager.h"
#include "RuntimeCreatedService.h"

using namespace Cppelix;


struct ITestService : virtual public IService {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class TestService : public ITestService, public Service {
public:
    ~TestService() final = default;
    bool start() final {
        LOG_INFO(_logger, "TestService started with dependency");
        _manager->pushEvent<QuitEvent>(getServiceId(), this);
        return true;
    }

    bool stop() final {
        LOG_INFO(_logger, "TestService stopped with dependency");
        return true;
    }

    void addDependencyInstance(IFrameworkLogger *logger) {
        _logger = logger;
        LOG_INFO(_logger, "Inserted logger");
    }

    void removeDependencyInstance(IFrameworkLogger *logger) {
        _logger = nullptr;
    }

    void addDependencyInstance(IRuntimeCreatedService *) {
        LOG_INFO(_logger, "Inserted IRuntimeCreatedService");
    }

    void removeDependencyInstance(IRuntimeCreatedService *) {
    }

private:
    IFrameworkLogger *_logger;
};