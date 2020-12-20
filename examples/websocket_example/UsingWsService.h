#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/optional_bundles/timer_bundle/TimerService.h>
#include <ichor/optional_bundles/network_bundle/NetworkDataEvent.h>
#include <ichor/optional_bundles/network_bundle/IConnectionService.h>
#include <ichor/optional_bundles/network_bundle/IHostService.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include <ichor/optional_bundles/serialization_bundle/ISerializationAdmin.h>
#include "../common/TestMsg.h"

using namespace Ichor;

class UsingWsService final : public Service {
public:
    UsingWsService(DependencyRegister &reg, IchorProperties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<ISerializationAdmin>(this, true);
        reg.registerDependency<IConnectionService>(this, true, *getProperties());
    }
    ~UsingWsService() final = default;

    bool start() final {
        LOG_INFO(_logger, "UsingWsService started");
        _timerEventRegistration = getManager()->registerEventHandler<NetworkDataEvent>(this);
        _connectionService->send(_serializationAdmin->serialize(TestMsg{11, "hello"}));
        return true;
    }

    bool stop() final {
        _timerEventRegistration = nullptr;
        LOG_INFO(_logger, "UsingWsService stopped");
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

    void addDependencyInstance(ISerializationAdmin *serializationAdmin) {
        _serializationAdmin = serializationAdmin;
        LOG_INFO(_logger, "Inserted serializationAdmin");
    }

    void removeDependencyInstance(ISerializationAdmin *serializationAdmin) {
        _serializationAdmin = nullptr;
        LOG_INFO(_logger, "Removed serializationAdmin");
    }

    void addDependencyInstance(IConnectionService *connectionService) {
        _connectionService = connectionService;
        LOG_INFO(_logger, "Inserted connectionService");
    }

    void addDependencyInstance(IHostService *) {
    }

    void removeDependencyInstance(IHostService *) {
    }

    void removeDependencyInstance(IConnectionService *connectionService) {
        LOG_INFO(_logger, "Removed connectionService");
    }

    Generator<bool> handleEvent(NetworkDataEvent const * const evt) {
        auto msg = _serializationAdmin->deserialize<TestMsg>(evt->getData());
        LOG_INFO(_logger, "Received TestMsg id {} val {}", msg->id, msg->val);
        getManager()->pushEvent<QuitEvent>(getServiceId());

        co_return (bool)PreventOthersHandling;
    }

private:
    ILogger *_logger{nullptr};
    ISerializationAdmin *_serializationAdmin{nullptr};
    IConnectionService *_connectionService{nullptr};
    std::unique_ptr<EventHandlerRegistration> _timerEventRegistration{nullptr};
};