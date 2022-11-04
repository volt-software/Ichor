#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/TimerService.h>
#include <ichor/services/network/NetworkEvents.h>
#include <ichor/services/network/http/IHttpConnectionService.h>
#include <ichor/services/network/http/IHttpService.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include <ichor/services/serialization/ISerializationAdmin.h>
#include "PingMsg.h"

using namespace Ichor;

class PongService final : public Service<PongService> {
public:
    PongService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<ISerializationAdmin>(this, true);
        reg.registerDependency<IHttpService>(this, true);
    }
    ~PongService() final = default;

private:
    StartBehaviour start() final {
        ICHOR_LOG_INFO(_logger, "PongService started");

        return StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        _routeRegistration.reset();
        ICHOR_LOG_INFO(_logger, "PongService stopped");
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

    void addDependencyInstance(IHttpService *svc, IService *) {
        _routeRegistration = svc->addRoute(HttpMethod::post, "/ping", [this](HttpRequest &req) -> AsyncGenerator<HttpResponse> {
            auto msg = _serializationAdmin->deserialize<PingMsg>(std::move(req.body));
            ICHOR_LOG_WARN(_logger, "received request from {} on route {} {} with PingMsg {}", req.address, (int) req.method, req.route, msg->sequence);
            co_return HttpResponse{false, HttpStatus::ok, _serializationAdmin->serialize(PingMsg{msg->sequence}), {}};
        });
    }

    void removeDependencyInstance(IHttpService *, IService *) {
        _routeRegistration.reset();
    }

    friend DependencyRegister;
    friend DependencyManager;

    ILogger *_logger{nullptr};
    ISerializationAdmin *_serializationAdmin{nullptr};
    std::unique_ptr<HttpRouteRegistration> _routeRegistration{nullptr};
};