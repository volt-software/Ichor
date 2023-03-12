#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/services/network/NetworkEvents.h>
#include <ichor/services/network/IConnectionService.h>
#include <ichor/services/network/IHostService.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/serialization/ISerializer.h>
#include "../common/TestMsg.h"

using namespace Ichor;

class UsingWsService final : public AdvancedService<UsingWsService> {
public:
    UsingWsService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<ISerializer<TestMsg>>(this, true);
        reg.registerDependency<IConnectionService>(this, true, getProperties());
    }
    ~UsingWsService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "UsingWsService started");
        _dataEventRegistration = GetThreadLocalManager().registerEventHandler<NetworkDataEvent>(this, this);
        _failureEventRegistration = GetThreadLocalManager().registerEventHandler<FailedSendMessageEvent>(this, this);
        _connectionService->sendAsync(_serializer->serialize(TestMsg{11, "hello"}));
        co_return {};
    }

    Task<void> stop() final {
        _dataEventRegistration.reset();
        _failureEventRegistration.reset();
        ICHOR_LOG_INFO(_logger, "UsingWsService stopped");
        co_return;
    }

    void addDependencyInstance(ILogger &logger, IService &) {
        _logger = &logger;
    }

    void removeDependencyInstance(ILogger&, IService&) {
        _logger = nullptr;
    }

    void addDependencyInstance(ISerializer<TestMsg> &serializer, IService&) {
        _serializer = &serializer;
        ICHOR_LOG_INFO(_logger, "Inserted serializer");
    }

    void removeDependencyInstance(ISerializer<TestMsg>&, IService&) {
        _serializer = nullptr;
        ICHOR_LOG_INFO(_logger, "Removed serializer");
    }

    void addDependencyInstance(IConnectionService &connectionService, IService&) {
        _connectionService = &connectionService;
        ICHOR_LOG_INFO(_logger, "Inserted connectionService");
    }

    void removeDependencyInstance(IConnectionService&, IService&) {
        ICHOR_LOG_INFO(_logger, "Removed connectionService");
    }

    void addDependencyInstance(IHostService&, IService&) {
    }

    void removeDependencyInstance(IHostService&, IService&) {
    }

    AsyncGenerator<IchorBehaviour> handleEvent(NetworkDataEvent const &evt) {
        auto msg = _serializer->deserialize(std::vector<uint8_t>{evt.getData()});
        ICHOR_LOG_INFO(_logger, "Received TestMsg id {} val {}", msg->id, msg->val);
        GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());

        co_return {};
    }

    AsyncGenerator<IchorBehaviour> handleEvent(FailedSendMessageEvent const &evt) {
        ICHOR_LOG_INFO(_logger, "Failed to send message id {}, retrying", evt.msgId);
        _connectionService->sendAsync(std::move(evt.data));

        co_return {};
    }

    friend DependencyRegister;
    friend DependencyManager;

    ILogger *_logger{nullptr};
    ISerializer<TestMsg> *_serializer{nullptr};
    IConnectionService *_connectionService{nullptr};
    EventHandlerRegistration _dataEventRegistration{};
    EventHandlerRegistration _failureEventRegistration{};
};
