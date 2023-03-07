#pragma once

#include <thread>
#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/CommunicationChannel.h>
#include "CustomEvent.h"

using namespace Ichor;

class OneService final : public AdvancedService<OneService> {
public:
    OneService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~OneService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "OneService started with dependency");
        // this component sometimes starts up before the other thread has started the OtherService
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        GetThreadLocalManager().getCommunicationChannel()->broadcastEvent<CustomEvent>(GetThreadLocalManager(), getServiceId());
        co_return {};
    }

    Task<void> stop() final {
        ICHOR_LOG_INFO(_logger, "OneService stopped with dependency");
        co_return;
    }

    void addDependencyInstance(ILogger &logger, IService &isvc) {
        _logger = &logger;

        ICHOR_LOG_INFO(_logger, "Inserted logger svcid {} for svcid {}", isvc.getServiceId(), getServiceId());
    }

    void removeDependencyInstance(ILogger&, IService&) {
        _logger = nullptr;
    }

    friend DependencyRegister;

    ILogger *_logger{};
};
