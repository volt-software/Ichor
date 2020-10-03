#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include "CustomEvent.h"

using namespace Ichor;

struct IOtherService : virtual public IService {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class OtherService final : public IOtherService, public Service {
public:
    OtherService(DependencyRegister &reg, IchorProperties props) : Service(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~OtherService() final = default;
    bool start() final {
        LOG_INFO(_logger, "OtherService started with dependency");
        _customEventHandler = getManager()->registerEventHandler<CustomEvent>(getServiceId(), this);
        return true;
    }

    bool stop() final {
        LOG_INFO(_logger, "OtherService stopped with dependency");
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;

        LOG_INFO(_logger, "Inserted logger svcid {} for svcid {}", logger->getServiceId(), getServiceId());
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

    Generator<bool> handleEvent(CustomEvent const * const evt) {
        LOG_INFO(_logger, "Handling custom event");
        getManager()->pushEvent<QuitEvent>(getServiceId());
        getManager()->getCommunicationChannel()->broadcastEvent<QuitEvent>(getManager(), getServiceId(), INTERNAL_EVENT_PRIORITY+1);

        // we dealt with it, don't let other services handle this event
        co_return (bool)PreventOthersHandling;
    }

private:
    ILogger *_logger;
    std::unique_ptr<EventHandlerRegistration> _customEventHandler;
};