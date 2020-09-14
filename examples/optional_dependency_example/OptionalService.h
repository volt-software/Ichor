#pragma once

#include <cppelix/DependencyManager.h>
#include <cppelix/optional_bundles/logging_bundle/Logger.h>
#include <cppelix/Service.h>
#include <cppelix/LifecycleManager.h>

using namespace Cppelix;


struct IOptionalService : virtual public IService {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class OptionalService final : public IOptionalService, public Service {
public:
    OptionalService(DependencyRegister &reg, CppelixProperties props) : Service(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
    }

    ~OptionalService() final = default;
    bool start() final {
        LOG_INFO(_logger, "OptionalService {} started", getServiceId());
        return true;
    }

    bool stop() final {
        LOG_INFO(_logger, "OptionalService {} stopped", getServiceId());
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
    ILogger *_logger{nullptr};
};