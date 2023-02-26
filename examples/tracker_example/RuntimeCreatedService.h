#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>

using namespace Ichor;


struct IRuntimeCreatedService {
};

class RuntimeCreatedService final : public IRuntimeCreatedService, public AdvancedService<RuntimeCreatedService> {
public:
    RuntimeCreatedService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
    }

    ~RuntimeCreatedService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        auto const& scope = Ichor::any_cast<std::string&>(_properties["scope"]);
        ICHOR_LOG_INFO(_logger, "RuntimeCreatedService started with scope {}", scope);
        co_return {};
    }

    Task<void> stop() final {
        ICHOR_LOG_INFO(_logger, "RuntimeCreatedService stopped");
        co_return;
    }

    void addDependencyInstance(ILogger *logger, IService *isvc) {
        _logger = logger;
        ICHOR_LOG_INFO(_logger, "Inserted logger svcid {} for svcid {}", isvc->getServiceId(), getServiceId());
    }

    void removeDependencyInstance(ILogger *logger, IService *) {
        _logger = nullptr;
    }

    friend DependencyRegister;

    ILogger *_logger{nullptr};
};
