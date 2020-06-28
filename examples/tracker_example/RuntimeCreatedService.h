#pragma once

#include <spdlog/spdlog.h>
#include <framework/DependencyManager.h>
#include <framework/interfaces/IFrameworkLogger.h>
#include "framework/Service.h"
#include "framework/Framework.h"
#include "framework/ServiceLifecycleManager.h"

using namespace Cppelix;


struct IRuntimeCreatedService : virtual public IService {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class RuntimeCreatedService : public IRuntimeCreatedService, public Service {
public:
    ~RuntimeCreatedService() final = default;
    bool start() final {
        auto scope = _properties["scope"]->getAsString();
        LOG_INFO(_logger, "RuntimeCreatedService started with scope {}", scope);
        return true;
    }

    bool stop() final {
        LOG_INFO(_logger, "RuntimeCreatedService stopped");
        return true;
    }

    void addDependencyInstance(IFrameworkLogger *logger) {
        _logger = logger;
        LOG_INFO(_logger, "Inserted logger");
    }

    void removeDependencyInstance(IFrameworkLogger *logger) {
        _logger = nullptr;
    }

private:
    IFrameworkLogger *_logger;
};