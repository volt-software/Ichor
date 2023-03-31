#pragma once

#include <thread>
#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/CommunicationChannel.h>
#include "CustomEvent.h"

using namespace Ichor;

class OneService final {
public:
    OneService(DependencyManager *dm, IService *self, ILogger *logger) : _logger(logger) {
        ICHOR_LOG_INFO(_logger, "OneService started with dependency");
        // this component sometimes starts up before the other thread has started the OtherService
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        dm->getCommunicationChannel()->broadcastEvent<CustomEvent>(GetThreadLocalManager(), self->getServiceId());
    }
    ~OneService() {
        ICHOR_LOG_INFO(_logger, "OneService stopped with dependency");
    }

private:
    ILogger *_logger{};
};
