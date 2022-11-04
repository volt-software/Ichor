#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/TimerService.h>
#include <ichor/services/network/NetworkEvents.h>
#include <ichor/services/network/http/IHttpConnectionService.h>
#include <ichor/services/network/http/IHttpService.h>
#include <ichor/services/timer/TimerService.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include <ichor/services/serialization/ISerializationAdmin.h>
#include "PingMsg.h"

using namespace Ichor;

class PingService final : public Service<PingService> {
public:
    PingService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<ISerializationAdmin>(this, true);
        reg.registerDependency<IHttpConnectionService>(this, true, getProperties());
    }
    ~PingService() final = default;

private:
    StartBehaviour start() final {
        ICHOR_LOG_INFO(_logger, "PingService started");
        _failureEventRegistration = getManager().registerEventHandler<FailedSendMessageEvent>(this);

        _timer = getManager().createServiceManager<Timer, ITimer>();
        _timer->setCallback(this, [this](DependencyManager &dm) -> AsyncGenerator<void> {
            auto toSendMsg = _serializationAdmin->serialize(PingMsg{_sequence});

            _sequence++;
            auto start = std::chrono::steady_clock::now();
            auto &msg = *co_await sendTestRequest(std::move(toSendMsg)).begin();
            auto end = std::chrono::steady_clock::now();
            auto addr = Ichor::any_cast<std::string&>(getProperties()["Address"]);
            if(msg) {
                ICHOR_LOG_INFO(_logger, "{} bytes from {}: icmp_seq={}, time={:L} ms", sizeof(PingMsg), addr, msg->sequence,
                               std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.);
            } else {
                _failed++;
                ICHOR_LOG_INFO(_logger, "Couldn't ping {}: total failed={}", addr, _failed);
            }
            co_return;
        });
        _timer->setChronoInterval(10ms);
        _timer->startTimer();

        return StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        _timer->stopTimer();
        _dataEventRegistration.reset();
        _failureEventRegistration.reset();
        ICHOR_LOG_INFO(_logger, "PingService stopped");
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

    void addDependencyInstance(IHttpConnectionService *connectionService, IService *) {
        _connectionService = connectionService;
        ICHOR_LOG_INFO(_logger, "Inserted IHttpConnectionService");
    }

    void removeDependencyInstance(IHttpConnectionService *connectionService, IService *) {
        ICHOR_LOG_INFO(_logger, "Removed IHttpConnectionService");
    }

    AsyncGenerator<void> handleEvent(FailedSendMessageEvent const &evt) {
        ++_failed;
        ICHOR_LOG_INFO(_logger, "Failed to send message id {}, total failed {}", evt.msgId, _failed);
        co_return;
    }

    friend DependencyRegister;
    friend DependencyManager;

    AsyncGenerator<std::unique_ptr<PingMsg>> sendTestRequest(std::vector<uint8_t> &&toSendMsg) {
        auto &response = *co_await _connectionService->sendAsync(HttpMethod::post, "/ping", {}, std::move(toSendMsg)).begin();

        if(response.status == HttpStatus::ok) {
            auto msg = _serializationAdmin->deserialize<PingMsg>(response.body);
//            ICHOR_LOG_INFO(_logger, "Received PingMsg sequence {}", msg->sequence);
            co_return msg;
        } else {
            ICHOR_LOG_ERROR(_logger, "Received status {}", (int)response.status);
            co_return std::unique_ptr<PingMsg>{};
        }
    }

    ILogger *_logger{nullptr};
    Timer *_timer{nullptr};
    ISerializationAdmin *_serializationAdmin{nullptr};
    IHttpConnectionService *_connectionService{nullptr};
    EventHandlerRegistration _dataEventRegistration{};
    EventHandlerRegistration _failureEventRegistration{};
    uint64_t _sequence{};
    uint64_t _failed{};
};