#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/network/IConnectionService.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/services/serialization/ISerializer.h>
#include <ichor/events/RunFunctionEvent.h>
#include "../common/TestMsg.h"
#include <ichor/ServiceExecutionScope.h>

using namespace Ichor;
using namespace Ichor::v1;

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

    void addDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService &) {
        _logger = std::move(logger);
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService&) {
        _logger.reset();
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<ISerializer<TestMsg>*> serializer, IService&) {
        _serializer = std::move(serializer);
        ICHOR_LOG_INFO(_logger, "Inserted serializer");
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<ISerializer<TestMsg>*>, IService&) {
        _serializer = nullptr;
        ICHOR_LOG_INFO(_logger, "Removed serializer");
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<IConnectionService*> connectionService, IService &svc) {
        if(connectionService->isClient()) {
            _clientService = std::move(connectionService);
            ICHOR_LOG_INFO(_logger, "Inserted clientService");
        } else {
            _hostService = std::move(connectionService);
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

    void removeDependencyInstance(Ichor::ScopedServiceProxy<IConnectionService*>, IService&) {
        ICHOR_LOG_INFO(_logger, "Removed connectionService");
    }

    friend DependencyRegister;
    friend DependencyManager;

    Ichor::ScopedServiceProxy<ILogger*> _logger {};
    Ichor::ScopedServiceProxy<ISerializer<TestMsg>*> _serializer {};
    Ichor::ScopedServiceProxy<IConnectionService*> _clientService {};
    Ichor::ScopedServiceProxy<IConnectionService*> _hostService {};
};
