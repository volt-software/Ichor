#pragma once

#include <framework/DependencyManager.h>
#include <optional_bundles/logging_bundle/Logger.h>
#include "framework/Service.h"
#include "framework/LifecycleManager.h"
#include "framework/CommunicationChannel.h"
#include "CustomEvent.h"

using namespace Cppelix;


struct IOneService : virtual public IService {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class OneService final : public IOneService, public Service {
public:
    ~OneService() final = default;
    bool start() final {
        LOG_INFO(_logger, "OneService started with dependency");
        // this component sometimes starts up before the other thread has started the OtherService
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        getManager()->getCommunicationChannel()->broadcastEvent<CustomEvent>(getManager(), getServiceId());
        return true;
    }

    bool stop() final {
        LOG_INFO(_logger, "OneService stopped with dependency");
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
    ILogger *_logger;
};