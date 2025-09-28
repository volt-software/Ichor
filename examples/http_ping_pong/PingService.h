#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/services/network/http/IHttpConnectionService.h>
#include <ichor/services/network/http/IHttpHostService.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/services/serialization/ISerializer.h>
#include "PingMsg.h"
#include <ichor/ServiceExecutionScope.h>

using namespace Ichor;
using namespace Ichor::v1;

class PingService final : public AdvancedService<PingService> {
public:
    PingService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED, Properties{{"LogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_INFO)}}); // using customer properties here prevents us refactoring this class to constructor injection
        reg.registerDependency<ISerializer<PingMsg>>(this, DependencyFlags::REQUIRED);
        reg.registerDependency<IHttpConnectionService>(this, DependencyFlags::REQUIRED, getProperties()); // using getProperties here prevents us refactoring this class to constructor injection
        reg.registerDependency<ITimerFactory>(this, DependencyFlags::REQUIRED);
    }
    ~PingService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "PingService started");

        auto timer = _timerFactory->createTimer();
        timer.setCallbackAsync([this, &timer = timer]() -> AsyncGenerator<IchorBehaviour> {
            auto toSendMsg = _serializer->serialize(PingMsg{_sequence});

            _sequence++;
            auto start = std::chrono::steady_clock::now();
            auto msg = co_await sendTestRequest(std::move(toSendMsg));
            auto end = std::chrono::steady_clock::now();
            auto &addr = Ichor::v1::any_cast<std::string&>(getProperties()["Address"]);
            if(msg) {
                ICHOR_LOG_INFO(_logger, "{} bytes from {}: icmp_seq={}, time={:L} ms", sizeof(PingMsg), addr, msg->sequence,
                               static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) / 1000.);
            } else {
                _failed++;
                ICHOR_LOG_INFO(_logger, "Couldn't ping {}: total failed={}", addr, _failed);
            }

            // prevent sending more than we can handle
            if(std::chrono::duration_cast<std::chrono::microseconds>(end - start) > _timerTimeout) {
                _timerTimeout = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                timer.setChronoInterval(_timerTimeout);
            }

            co_return {};
        });
        timer.setChronoInterval(_timerTimeout);
        timer.startTimer();

        co_return {};
    }

    Task<void> stop() final {
        ICHOR_LOG_INFO(_logger, "PingService stopped");
        co_return;
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService &) {
        _logger = std::move(logger);
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<ILogger*>, IService&) {
        _logger.reset();
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<ISerializer<PingMsg>*> serializer, IService&) {
        _serializer = std::move(serializer);
        ICHOR_LOG_INFO(_logger, "Inserted serializer");
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<ISerializer<PingMsg>*>, IService&) {
        _serializer = nullptr;
        ICHOR_LOG_INFO(_logger, "Removed serializer");
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<IHttpConnectionService*> connectionService, IService&) {
        _connectionService = std::move(connectionService);
        ICHOR_LOG_INFO(_logger, "Inserted IHttpConnectionService");
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<IHttpConnectionService*>, IService&) {
        ICHOR_LOG_INFO(_logger, "Removed IHttpConnectionService");
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<ITimerFactory*> factory, IService &) {
        _timerFactory = std::move(factory);
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<ITimerFactory*> factory, IService&) {
        _timerFactory.reset();
    }

    friend DependencyRegister;

    Task<tl::optional<PingMsg>> sendTestRequest(std::vector<uint8_t> &&toSendMsg) {
        unordered_map<std::string, std::string> headers{{"Content-Type", "application/json"}};
        auto response = co_await _connectionService->sendAsync(HttpMethod::post, "/ping", std::move(headers), std::move(toSendMsg));

        if(!response) {
            ICHOR_LOG_ERROR(_logger, "Http send error {}", response.error());
            co_return tl::optional<PingMsg>{};
        }

        if(_serializer == nullptr) {
            // we're stopping, gotta bail.
            co_return tl::optional<PingMsg>{};
        }

        if(response->status == HttpStatus::ok) {
            auto msg = _serializer->deserialize(response->body);
            co_return msg;
        } else {
            ICHOR_LOG_ERROR(_logger, "Received status {}", (int)response->status);
            co_return tl::optional<PingMsg>{};
        }
    }

    Ichor::ScopedServiceProxy<ILogger*> _logger {};
    Ichor::ScopedServiceProxy<ITimerFactory*> _timerFactory {};
    Ichor::ScopedServiceProxy<ISerializer<PingMsg>*> _serializer {};
    Ichor::ScopedServiceProxy<IHttpConnectionService*> _connectionService {};
    uint64_t _sequence{};
    uint64_t _failed{};
    std::chrono::milliseconds _timerTimeout{10ms};
};
