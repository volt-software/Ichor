#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/TimerService.h>
#include <ichor/services/network/NetworkEvents.h>
#include <ichor/services/network/http/IHttpConnectionService.h>
#include <ichor/services/network/http/IHttpService.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/ILifecycleManager.h>
#include <ichor/services/serialization/ISerializer.h>
#include "../common/TestMsg.h"

using namespace Ichor;

class UsingHttpService final : public AdvancedService<UsingHttpService> {
public:
    UsingHttpService(DependencyRegister &reg, Properties props, DependencyManager *mng) : AdvancedService(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<ISerializer<TestMsg>>(this, true);
        reg.registerDependency<IHttpConnectionService>(this, true, getProperties());
        reg.registerDependency<IHttpService>(this, true);
    }
    ~UsingHttpService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "UsingHttpService started");

        getManager().pushEvent<RunFunctionEventAsync>(getServiceId(), [this](DependencyManager &dm) -> AsyncGenerator<IchorBehaviour> {
            auto toSendMsg = _serializer->serialize(TestMsg{11, "hello"});

            co_await sendTestRequest(std::move(toSendMsg)).begin();
            co_return {};
        });

        co_return {};
    }

    Task<void> stop() final {
        _routeRegistration.reset();
        ICHOR_LOG_INFO(_logger, "UsingHttpService stopped");
        co_return;
    }

    void addDependencyInstance(ILogger *logger, IService *) {
        _logger = logger;
    }

    void removeDependencyInstance(ILogger *logger, IService *) {
        _logger = nullptr;
    }

    void addDependencyInstance(ISerializer<TestMsg> *serializer, IService *) {
        _serializer = serializer;
        ICHOR_LOG_INFO(_logger, "Inserted serializer");
    }

    void removeDependencyInstance(ISerializer<TestMsg> *serializer, IService *) {
        _serializer = nullptr;
        ICHOR_LOG_INFO(_logger, "Removed serializer");
    }

    void addDependencyInstance(IHttpConnectionService *connectionService, IService *) {
        _connectionService = connectionService;
        ICHOR_LOG_INFO(_logger, "Inserted IHttpConnectionService");
    }

    void addDependencyInstance(IHttpService *svc, IService *) {
        ICHOR_LOG_INFO(_logger, "Inserted IHttpService");
        _routeRegistration = svc->addRoute(HttpMethod::post, "/test", [this](HttpRequest &req) -> AsyncGenerator<HttpResponse> {
            auto msg = _serializer->deserialize(std::move(req.body));
            ICHOR_LOG_WARN(_logger, "received request on route {} {} with testmsg {} - {}", (int)req.method, req.route, msg->id, msg->val);
            co_return HttpResponse{false, HttpStatus::ok, "application/json", _serializer->serialize(TestMsg{11, "hello"}), {}};
        });
    }

    void removeDependencyInstance(IHttpService *, IService *) {
        ICHOR_LOG_INFO(_logger, "Removed IHttpService");
        _routeRegistration.reset();
    }

    void removeDependencyInstance(IHttpConnectionService *connectionService, IService *) {
        ICHOR_LOG_INFO(_logger, "Removed IHttpConnectionService");
    }

    friend DependencyRegister;
    friend DependencyManager;

    AsyncGenerator<void> sendTestRequest(std::vector<uint8_t> &&toSendMsg) {
        ICHOR_LOG_INFO(_logger, "sendTestRequest");
        std::vector<HttpHeader> headers{HttpHeader{"Content-Type", "application/json"}};
        auto response = co_await _connectionService->sendAsync(HttpMethod::post, "/test", std::move(headers), std::move(toSendMsg));

        if(_serializer == nullptr) {
            // we're stopping, gotta bail.
            co_return;
        }

        if(response.status == HttpStatus::ok) {
            auto msg = _serializer->deserialize(std::move(response.body));
            ICHOR_LOG_INFO(_logger, "Received TestMsg id {} val {}", msg->id, msg->val);
        } else {
            ICHOR_LOG_ERROR(_logger, "Received status {}", (int)response.status);
        }
        getManager().pushEvent<QuitEvent>(getServiceId());

        co_return;
    }

    ILogger *_logger{nullptr};
    ISerializer<TestMsg> *_serializer{nullptr};
    IHttpConnectionService *_connectionService{nullptr};
    std::unique_ptr<HttpRouteRegistration> _routeRegistration{nullptr};
};
