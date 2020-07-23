#pragma once

#include <framework/DependencyManager.h>
#include <optional_bundles/logging_bundle/Logger.h>
#include "framework/Service.h"
#include "framework/ServiceLifecycleManager.h"
#include "CustomEvent.h"

using namespace Cppelix;

struct IOtherService : virtual public IService {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class OtherService final : public IOtherService, public Service {
public:
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

    cppcoro::generator<EventCallbackReturn> handleEvent(CustomEvent const * const evt) {
        getManager()->pushEvent<QuitEvent>(getServiceId(), this);
        getManager()->getCommunicationChannel()->broadcastEvent<QuitEvent>(getManager(), getServiceId());

        // we dealt with it, don't let other services handle this event
        co_yield EventCallbackReturn{false, false};
    }

private:
    ILogger *_logger;
    std::unique_ptr<EventHandlerRegistration> _customEventHandler;
};