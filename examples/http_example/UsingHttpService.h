#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/optional_bundles/timer_bundle/TimerService.h>
#include <ichor/optional_bundles/network_bundle/NetworkDataEvent.h>
#include <ichor/optional_bundles/network_bundle/http/IHttpConnectionService.h>
#include <ichor/optional_bundles/network_bundle/http/IHttpService.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include <ichor/optional_bundles/serialization_bundle/ISerializationAdmin.h>
#include "../common/TestMsg.h"

using namespace Ichor;

class UsingHttpService final : public Service<UsingHttpService> {
public:
    UsingHttpService(DependencyRegister &reg, IchorProperties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<ISerializationAdmin>(this, true);
        reg.registerDependency<IHttpConnectionService>(this, true, *getProperties());
        reg.registerDependency<IHttpService>(this, true);
    }
    ~UsingHttpService() final = default;

    bool start() final {
        ICHOR_LOG_INFO(_logger, "UsingHttpService started");
        _timerEventRegistration = getManager()->registerEventHandler<HttpResponseEvent>(this);
        _connectionService->sendAsync(HttpMethod::post, "/test", {}, _serializationAdmin->serialize(TestMsg{11, "hello"}));
        return true;
    }

    bool stop() final {
        _timerEventRegistration = nullptr;
        _routeRegistration = nullptr;
        ICHOR_LOG_INFO(_logger, "UsingHttpService stopped");
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
        ICHOR_LOG_INFO(_logger, "Inserted serializationAdmin");
    }

    void removeDependencyInstance(ISerializationAdmin *serializationAdmin) {
        _serializationAdmin = nullptr;
        ICHOR_LOG_INFO(_logger, "Removed serializationAdmin");
    }

    void addDependencyInstance(IHttpConnectionService *connectionService) {
        _connectionService = connectionService;
        ICHOR_LOG_INFO(_logger, "Inserted IHttpConnectionService");
    }

    void addDependencyInstance(IHttpService *svc) {
        _routeRegistration = svc->addRoute(HttpMethod::post, "/test", [this](HttpRequest &req) -> HttpResponse{
            auto msg = _serializationAdmin->deserialize<TestMsg>(std::move(req.body));
            ICHOR_LOG_WARN(_logger, "received request on route {} {} with testmsg {} - {}", req.method, req.route, msg->id, msg->val);
            return HttpResponse{HttpStatus::ok, _serializationAdmin->serialize(TestMsg{11, "hello"}), {}};
        });
    }

    void removeDependencyInstance(IHttpService *) {
        _routeRegistration = nullptr;
    }

    void removeDependencyInstance(IHttpConnectionService *connectionService) {
        ICHOR_LOG_INFO(_logger, "Removed IHttpConnectionService");
    }

    Generator<bool> handleEvent(HttpResponseEvent const * const evt) {
        if(evt->response.status == HttpStatus::ok) {
            auto msg = _serializationAdmin->deserialize<TestMsg>(evt->response.body);
            ICHOR_LOG_INFO(_logger, "Received TestMsg id {} val {}", msg->id, msg->val);
        } else {
            ICHOR_LOG_ERROR(_logger, "Received status {}", evt->response.status);
        }
        getManager()->pushEvent<QuitEvent>(getServiceId());

        co_return (bool)PreventOthersHandling;
    }

private:
    ILogger *_logger{nullptr};
    ISerializationAdmin *_serializationAdmin{nullptr};
    IHttpConnectionService *_connectionService{nullptr};
    std::unique_ptr<EventHandlerRegistration, Deleter> _timerEventRegistration{nullptr};
    std::unique_ptr<HttpRouteRegistration, Deleter> _routeRegistration{nullptr};
};