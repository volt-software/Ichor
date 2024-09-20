#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/network/IConnectionService.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/services/serialization/ISerializer.h>
#include <ichor/events/RunFunctionEvent.h>
#include "../common/TestMsg.h"

using namespace Ichor;

class UsingTcpService final : public AdvancedService<UsingTcpService> {
public:
    UsingTcpService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
        reg.registerDependency<ISerializer<TestMsg>>(this, DependencyFlags::REQUIRED);
        reg.registerDependency<IConnectionService>(this, DependencyFlags::REQUIRED | DependencyFlags::ALLOW_MULTIPLE, getProperties());
    }
    ~UsingTcpService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "UsingTcpService started");
        co_return {};
    }

    Task<void> stop() final {
        ICHOR_LOG_INFO(_logger, "UsingTcpService stopped");
        co_return;
    }

    void addDependencyInstance(ILogger &logger, IService &) {
        _logger = &logger;
    }

    void removeDependencyInstance(ILogger &logger, IService&) {
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

    void addDependencyInstance(IConnectionService &connectionService, IService &svc) {
        if(connectionService.isClient()) {
            _clientService = &connectionService;
            ICHOR_LOG_INFO(_logger, "Inserted clientService");
        } else {
            _hostService = &connectionService;
            ICHOR_LOG_INFO(_logger, "Inserted _hostService");
            _hostService->setReceiveHandler([this](std::span<uint8_t const> data) {
                auto msg = _serializer->deserialize(data);
                if(msg) {
//                    ICHOR_LOG_INFO(_logger, "Received TestMsg id {} val {}", msg->id, msg->val);
                    ICHOR_LOG_INFO(_logger, "Received TestMsg id {}", msg->id);
                } else {
                    ICHOR_LOG_ERROR(_logger, "Couldn't deserialize message {}", std::string_view{reinterpret_cast<char const *>(data.data()), data.size()});
                    std::terminate();
                }
                GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());
            });
        }

        if(_clientService != nullptr && _hostService != nullptr) {
            GetThreadLocalEventQueue().pushEvent<RunFunctionEventAsync>(getServiceId(), [this]() -> AsyncGenerator<IchorBehaviour> {
				auto ser = _serializer->serialize(TestMsg{11, "hello world"});
				auto ret = co_await _clientService->sendAsync(std::move(ser));
				if(!ret) {
					ICHOR_LOG_ERROR(_logger, "start() send error: {}", (int)ret.error());
				}
                co_return {};
            });
        }
    }

    void removeDependencyInstance(IConnectionService&, IService&) {
        ICHOR_LOG_INFO(_logger, "Removed connectionService");
    }

    friend DependencyRegister;
    friend DependencyManager;

    ILogger *_logger{};
    ISerializer<TestMsg> *_serializer{};
    IConnectionService *_clientService{};
    IConnectionService *_hostService{};
};
