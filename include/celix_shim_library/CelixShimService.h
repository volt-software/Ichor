#pragma once

#include <spdlog/spdlog.h>
#include <framework/DependencyManager.h>
#include <optional_bundles/logging_bundle/Logger.h>
#include <framework/Service.h>

using namespace Cppelix;

struct ICelixShimService : virtual public IService {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class CelixShimService final : public ICelixShimService, public Service {
public:
    ~CelixShimService() final = default;
    bool start() final {
        _celixService = std::any_cast<void*>(_properties["CelixService"]);
        LOG_INFO(_logger, "CelixShimService started with dependency");
        return true;
    }

    bool stop() final {
        LOG_INFO(_logger, "CelixShimService stopped with dependency");
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;

        LOG_INFO(_logger, "Inserted logger svcid {} for svcid {}", logger->getServiceId(), getServiceId());
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

private:
    ILogger *_logger;
    void *_celixService;
};