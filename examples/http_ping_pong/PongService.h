#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/services/network/NetworkEvents.h>
#include <ichor/services/network/http/IHttpConnectionService.h>
#include <ichor/services/network/http/IHttpHostService.h>
#include <ichor/services/serialization/ISerializer.h>
#include <ichor/coroutines/AsyncGenerator.h>
#include "PingMsg.h"

using namespace Ichor;

class PongService final {
public:
    PongService(ILogger *logger, ISerializer<PingMsg> *serializer, IHttpHostService *hostService) : _logger(logger) {
        _routeRegistration = hostService->addRoute(HttpMethod::post, "/ping", [this, serializer](HttpRequest &req) -> AsyncGenerator<HttpResponse> {
            ICHOR_LOG_INFO(_logger, "received request from {} with body {} ", req.address, std::string_view{reinterpret_cast<char*>(req.body.data()), req.body.size()});
            auto msg = serializer->deserialize(std::move(req.body));
            ICHOR_LOG_INFO(_logger, "received request from {} on route {} {} with PingMsg {}", req.address, (int) req.method, req.route, msg->sequence);
            co_return HttpResponse{false, HttpStatus::ok, "application/json", serializer->serialize(PingMsg{msg->sequence}), {}};
        });
    }
    ~PongService() {
        ICHOR_LOG_INFO(_logger, "PongService stopped");
    }

private:
    ILogger *_logger{};
    std::unique_ptr<HttpRouteRegistration> _routeRegistration{};
};
