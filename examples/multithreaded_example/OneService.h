#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include <ichor/CommunicationChannel.h>
#include "CustomEvent.h"

using namespace Ichor;

class OneService final : public Service<OneService> {
public:
    OneService(DependencyRegister &reg, IchorProperties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~OneService() final = default;
    bool start() final {
        ICHOR_LOG_INFO(_logger, "OneService started with dependency");
        // this component sometimes starts up before the other thread has started the OtherService
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        getManager()->getCommunicationChannel()->broadcastEvent<CustomEvent>(getManager(), getServiceId());
        return true;
    }

    bool stop() final {
        ICHOR_LOG_INFO(_logger, "OneService stopped with dependency");
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;

        ICHOR_LOG_INFO(_logger, "Inserted logger svcid {} for svcid {}", logger->getServiceId(), getServiceId());
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

private:
    ILogger *_logger;
};