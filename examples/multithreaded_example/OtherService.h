#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include "CustomEvent.h"

using namespace Ichor;

class OtherService final {
public:
    OtherService(DependencyManager *dm, IService *self, ILogger *logger) : _dm(dm), _self(self), _logger(logger) {
        ICHOR_LOG_INFO(_logger, "OtherService started with dependency");
        _customEventHandler = dm->registerEventHandler<CustomEvent, OtherService>(this, self);
    }
    ~OtherService() {
        ICHOR_LOG_INFO(_logger, "OtherService stopped with dependency");
    }

    AsyncGenerator<IchorBehaviour> handleEvent(CustomEvent const &) const {
        ICHOR_LOG_INFO(_logger, "Handling custom event");
        _dm->getEventQueue().pushEvent<QuitEvent>(_self->getServiceId());
        _dm->getCommunicationChannel()->broadcastEvent<QuitEvent>(GetThreadLocalManager(), _self->getServiceId());
        co_return {};
    }

private:
    DependencyManager *_dm{};
    IService *_self{};
    ILogger *_logger{};
    EventHandlerRegistration _customEventHandler{};
};
