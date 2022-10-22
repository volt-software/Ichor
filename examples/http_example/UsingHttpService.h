#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/optional_bundles/timer_bundle/TimerService.h>
#include <ichor/optional_bundles/network_bundle/NetworkEvents.h>
#include <ichor/optional_bundles/network_bundle/http/IHttpConnectionService.h>
#include <ichor/optional_bundles/network_bundle/http/IHttpService.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include <ichor/optional_bundles/serialization_bundle/ISerializationAdmin.h>
#include "../common/TestMsg.h"

using namespace Ichor;

class UsingHttpService final : public Service<UsingHttpService> {
public:
    UsingHttpService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<ISerializationAdmin>(this, true);
        reg.registerDependency<IHttpConnectionService>(this, true, getProperties());
        reg.registerDependency<IHttpService>(this, true);
    }
    ~UsingHttpService() final = default;

    StartBehaviour start() final {
        ICHOR_LOG_INFO(_logger, "UsingHttpService started");
        _dataEventRegistration = getManager()->registerEventHandler<HttpResponseEvent>(this);
        _failureEventRegistration = getManager()->registerEventHandler<FailedSendMessageEvent>(this);
        _connectionService->sendAsync(HttpMethod::post, "/test", {}, _serializationAdmin->serialize(TestMsg{11, "hello"}));
        return StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        _dataEventRegistration.reset();
        _failureEventRegistration.reset();
        _routeRegistration.reset();
        ICHOR_LOG_INFO(_logger, "UsingHttpService stopped");
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

    void addDependencyInstance(IHttpService *svc, IService *) {
        _routeRegistration = svc->addRoute(HttpMethod::post, "/test", [this](HttpRequest &req) -> HttpResponse{
            auto msg = _serializationAdmin->deserialize<TestMsg>(std::move(req.body));
            ICHOR_LOG_WARN(_logger, "received request on route {} {} with testmsg {} - {}", (int)req.method, req.route, msg->id, msg->val);
            return HttpResponse{HttpStatus::ok, _serializationAdmin->serialize(TestMsg{11, "hello"}), {}};
        });
    }

    void removeDependencyInstance(IHttpService *, IService *) {
        _routeRegistration.reset();
    }

    void removeDependencyInstance(IHttpConnectionService *connectionService, IService *) {
        ICHOR_LOG_INFO(_logger, "Removed IHttpConnectionService");
    }

    AsyncGenerator<bool> handleEvent(HttpResponseEvent const &evt) {
        if(evt.response.status == HttpStatus::ok) {
            auto msg = _serializationAdmin->deserialize<TestMsg>(evt.response.body);
            ICHOR_LOG_INFO(_logger, "Received TestMsg id {} val {}", msg->id, msg->val);
        } else {
            ICHOR_LOG_ERROR(_logger, "Received status {}", (int)evt.response.status);
        }
        getManager()->pushEvent<QuitEvent>(getServiceId());

        co_return (bool)PreventOthersHandling;
    }

    AsyncGenerator<bool> handleEvent(FailedSendMessageEvent const &evt) {
        ICHOR_LOG_INFO(_logger, "Failed to send message id {}, retrying", evt.msgId);
        _connectionService->sendAsync(HttpMethod::post, "/test", {}, std::move(evt.data));

        co_return (bool)PreventOthersHandling;
    }

private:
    ILogger *_logger{nullptr};
    ISerializationAdmin *_serializationAdmin{nullptr};
    IHttpConnectionService *_connectionService{nullptr};
    EventHandlerRegistration _dataEventRegistration{};
    EventHandlerRegistration _failureEventRegistration{};
    std::unique_ptr<HttpRouteRegistration> _routeRegistration{nullptr};
};