#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/optional_bundles/timer_bundle/TimerService.h>
#include <ichor/optional_bundles/network_bundle/NetworkEvents.h>
#include <ichor/optional_bundles/network_bundle/IConnectionService.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include <ichor/optional_bundles/serialization_bundle/ISerializationAdmin.h>
#include "../common/TestMsg.h"

using namespace Ichor;

class UsingTcpService final : public Service<UsingTcpService> {
public:
    UsingTcpService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<ISerializationAdmin>(this, true);
        reg.registerDependency<IConnectionService>(this, true, getProperties());
    }
    ~UsingTcpService() final = default;

    StartBehaviour start() final {
        ICHOR_LOG_INFO(_logger, "UsingTcpService started");
        _dataEventRegistration = getManager().registerEventHandler<NetworkDataEvent>(this);
        _failureEventRegistration = getManager().registerEventHandler<FailedSendMessageEvent>(this);
        _connectionService->sendAsync(_serializationAdmin->serialize(TestMsg{11, "hello"}));
        return StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        _dataEventRegistration.reset();
        _failureEventRegistration.reset();
        ICHOR_LOG_INFO(_logger, "UsingTcpService stopped");
        return StartBehaviour::SUCCEEDED;
    }

    void addDependencyInstance(ILogger *logger, IService *) {
        _logger = logger;
    }

    void removeDependencyInstance(ILogger *logger, IService *) {
        _logger = nullptr;
    }

    void addDependencyInstance(ISerializationAdmin *serializationAdmin, IService *) {
        _serializationAdmin = serializationAdmin;
        ICHOR_LOG_INFO(_logger, "Inserted serializationAdmin");
    }

    void removeDependencyInstance(ISerializationAdmin *serializationAdmin, IService *) {
        _serializationAdmin = nullptr;
        ICHOR_LOG_INFO(_logger, "Removed serializationAdmin");
    }

    void addDependencyInstance(IConnectionService *connectionService, IService *) {
        _connectionService = connectionService;
        ICHOR_LOG_INFO(_logger, "Inserted connectionService");
    }

    void removeDependencyInstance(IConnectionService *connectionService, IService *) {
        ICHOR_LOG_INFO(_logger, "Removed connectionService");
    }

    AsyncGenerator<bool> handleEvent(NetworkDataEvent const &evt) {
        auto msg = _serializationAdmin->deserialize<TestMsg>(evt.getData());
        ICHOR_LOG_INFO(_logger, "Received TestMsg id {} val {}", msg->id, msg->val);
        getManager().pushEvent<QuitEvent>(getServiceId());

        co_return (bool)PreventOthersHandling;
    }

    AsyncGenerator<bool> handleEvent(FailedSendMessageEvent const &evt) {
        ICHOR_LOG_INFO(_logger, "Failed to send message id {}, retrying", evt.msgId);
        _connectionService->sendAsync(std::move(evt.data));

        co_return (bool)PreventOthersHandling;
    }

private:
    ILogger *_logger{nullptr};
    ISerializationAdmin *_serializationAdmin{nullptr};
    IConnectionService *_connectionService{nullptr};
    EventHandlerRegistration _dataEventRegistration{};
    EventHandlerRegistration _failureEventRegistration{};
};