#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include "CustomEvent.h"

using namespace Ichor;

class OtherService final : public AdvancedService<OtherService> {
public:
    OtherService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~OtherService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "OtherService started with dependency");
        _customEventHandler = GetThreadLocalManager().registerEventHandler<CustomEvent>(this);
        co_return {};
    }

    Task<void> stop() final {
        ICHOR_LOG_INFO(_logger, "OtherService stopped with dependency");
        _customEventHandler.reset();
        co_return;
    }

    void addDependencyInstance(ILogger *logger, IService *isvc) {
        _logger = logger;

        ICHOR_LOG_INFO(_logger, "Inserted logger svcid {} for svcid {}", isvc->getServiceId(), getServiceId());
    }

    void removeDependencyInstance(ILogger *logger, IService *) {
        _logger = nullptr;
    }

    AsyncGenerator<IchorBehaviour> handleEvent(CustomEvent const &evt) {
        ICHOR_LOG_INFO(_logger, "Handling custom event");
        GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());
        GetThreadLocalManager().getCommunicationChannel()->broadcastEvent<QuitEvent>(GetThreadLocalManager(), getServiceId());
        co_return {};
    }

    friend DependencyRegister;
    friend DependencyManager;

    ILogger *_logger{};
    EventHandlerRegistration _customEventHandler{};
};
