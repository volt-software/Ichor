#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/ILifecycleManager.h>
#include "CustomEvent.h"

using namespace Ichor;

class OtherService final : public AdvancedService<OtherService> {
public:
    OtherService(DependencyRegister &reg, Properties props, DependencyManager *mng) : AdvancedService(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~OtherService() final = default;

private:
    AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "OtherService started with dependency");
        _customEventHandler = getManager().registerEventHandler<CustomEvent>(this);
        co_return {};
    }

    AsyncGenerator<void> stop() final {
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
        getManager().pushEvent<QuitEvent>(getServiceId());
        getManager().getCommunicationChannel()->broadcastEvent<QuitEvent>(getManager(), getServiceId());
        co_return {};
    }

    friend DependencyRegister;
    friend DependencyManager;

    ILogger *_logger{};
    EventHandlerRegistration _customEventHandler{};
};
