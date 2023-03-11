#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/services/network/NetworkEvents.h>
#include <ichor/services/network/http/IHttpConnectionService.h>
#include <ichor/services/network/http/IHttpService.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/services/serialization/ISerializer.h>
#include "PingMsg.h"

using namespace Ichor;

class PingService final : public AdvancedService<PingService> {
public:
    PingService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, true, Properties{{"LogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_INFO)}});
        reg.registerDependency<ISerializer<PingMsg>>(this, true);
        reg.registerDependency<IHttpConnectionService>(this, true, getProperties());
        reg.registerDependency<ITimerFactory>(this, true);
    }
    ~PingService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "PingService started");

        auto &timer = _timerFactory->createTimer();
        timer.setCallbackAsync([this, &timer = timer]() -> AsyncGenerator<IchorBehaviour> {
            auto toSendMsg = _serializer->serialize(PingMsg{_sequence});

            _sequence++;
            auto start = std::chrono::steady_clock::now();
            auto msg = co_await sendTestRequest(std::move(toSendMsg));
            auto end = std::chrono::steady_clock::now();
            auto &addr = Ichor::any_cast<std::string&>(getProperties()["Address"]);
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

    void addDependencyInstance(ILogger &logger, IService &) {
        _logger = &logger;
    }

    void removeDependencyInstance(ILogger&, IService&) {
        _logger = nullptr;
    }

    void addDependencyInstance(ISerializer<PingMsg> &serializer, IService&) {
        _serializer = &serializer;
        ICHOR_LOG_INFO(_logger, "Inserted serializer");
    }

    void removeDependencyInstance(ISerializer<PingMsg>&, IService&) {
        _serializer = nullptr;
        ICHOR_LOG_INFO(_logger, "Removed serializer");
    }

    void addDependencyInstance(IHttpConnectionService &connectionService, IService&) {
        _connectionService = &connectionService;
        ICHOR_LOG_INFO(_logger, "Inserted IHttpConnectionService");
    }

    void removeDependencyInstance(IHttpConnectionService&, IService&) {
        ICHOR_LOG_INFO(_logger, "Removed IHttpConnectionService");
    }

    void addDependencyInstance(ITimerFactory &factory, IService &) {
        _timerFactory = &factory;
    }

    void removeDependencyInstance(ITimerFactory &factory, IService&) {
        _timerFactory = nullptr;
    }

    friend DependencyRegister;

    Task<std::optional<PingMsg>> sendTestRequest(std::vector<uint8_t> &&toSendMsg) {
        std::vector<HttpHeader> headers{HttpHeader{"Content-Type", "application/json"}};
        auto response = co_await _connectionService->sendAsync(HttpMethod::post, "/ping", std::move(headers), std::move(toSendMsg));

        if(_serializer == nullptr) {
            // we're stopping, gotta bail.
            co_return std::optional<PingMsg>{};
        }

        if(response.status == HttpStatus::ok) {
            auto msg = _serializer->deserialize(std::move(response.body));
            co_return msg;
        } else {
            ICHOR_LOG_ERROR(_logger, "Received status {}", (int)response.status);
            co_return std::optional<PingMsg>{};
        }
    }

    ILogger *_logger{nullptr};
    ITimerFactory *_timerFactory{nullptr};
    ISerializer<PingMsg> *_serializer{nullptr};
    IHttpConnectionService *_connectionService{nullptr};
    uint64_t _sequence{};
    uint64_t _failed{};
    std::chrono::milliseconds _timerTimeout{10ms};
};
