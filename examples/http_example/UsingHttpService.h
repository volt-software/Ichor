#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/services/network/NetworkEvents.h>
#include <ichor/services/network/http/IHttpConnectionService.h>
#include <ichor/services/network/http/IHttpHostService.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/event_queues/IEventQueue.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/dependency_management/DependencyRegister.h>
#include <ichor/services/serialization/ISerializer.h>
#include "../common/TestMsg.h"

using namespace Ichor;

class UsingHttpService final : public AdvancedService<UsingHttpService> {
public:
    UsingHttpService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
        reg.registerDependency<ISerializer<TestMsg>>(this, DependencyFlags::REQUIRED);
        reg.registerDependency<IHttpConnectionService>(this, DependencyFlags::REQUIRED, getProperties()); // using getProperties here prevents us refactoring this class to constructor injection
        reg.registerDependency<IHttpHostService>(this, DependencyFlags::REQUIRED);
    }
    ~UsingHttpService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "UsingHttpService started");

        GetThreadLocalEventQueue().pushEvent<RunFunctionEventAsync>(getServiceId(), [this]() -> AsyncGenerator<IchorBehaviour> {
            auto toSendMsg = _serializer->serialize(TestMsg{11, "hello"});

            co_await sendTestRequest(std::move(toSendMsg));
            co_await sendRegexRequest();
            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());
            co_return {};
        });

        co_return {};
    }

    Task<void> stop() final {
        _routeRegistrations.clear();
        ICHOR_LOG_INFO(_logger, "UsingHttpService stopped");
        co_return;
    }

    void addDependencyInstance(ILogger &logger, IService &) {
        _logger = &logger;
    }

    void removeDependencyInstance(ILogger&, IService&) {
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

    void addDependencyInstance(IHttpConnectionService &connectionService, IService&) {
        _connectionService = &connectionService;
        ICHOR_LOG_INFO(_logger, "Inserted IHttpConnectionService");
    }

    void removeDependencyInstance(IHttpConnectionService&, IService&) {
        ICHOR_LOG_INFO(_logger, "Removed IHttpConnectionService");
    }

    void addDependencyInstance(IHttpHostService &svc, IService&) {
        ICHOR_LOG_INFO(_logger, "Inserted IHttpHostService");
        _routeRegistrations.emplace_back(svc.addRoute(HttpMethod::post, "/test", [this](HttpRequest &req) -> AsyncGenerator<HttpResponse> {
            auto msg = _serializer->deserialize(std::move(req.body));
            ICHOR_LOG_WARN(_logger, "received request on route {} {} with testmsg {} - {}", (int)req.method, req.route, msg->id, msg->val);
            co_return HttpResponse{HttpStatus::ok, "application/json", _serializer->serialize(TestMsg{11, "hello"}), {}};
        }));
        _routeRegistrations.emplace_back(svc.addRoute(HttpMethod::get, std::make_unique<RegexRouteMatch<R"(\/regex_test\/([a-zA-Z0-9]*)\?*([a-zA-Z0-9]+=[a-zA-Z0-9]+)*&*([a-zA-Z0-9]+=[a-zA-Z0-9]+)*)">>(), [this](HttpRequest &req) -> AsyncGenerator<HttpResponse> {
            ICHOR_LOG_WARN(_logger, "received request on route {} {} with params:", (int)req.method, req.route);
            for(auto const &param : req.regex_params) {
                ICHOR_LOG_WARN(_logger, "{}", param);
            }
            co_return HttpResponse{HttpStatus::ok, "text/plain", {}, {}};
        }));
    }

    void removeDependencyInstance(IHttpHostService&, IService&) {
        ICHOR_LOG_INFO(_logger, "Removed IHttpHostService");
        _routeRegistrations.clear();
    }

    friend DependencyRegister;

    Task<void> sendTestRequest(std::vector<uint8_t> &&toSendMsg) {
        ICHOR_LOG_INFO(_logger, "sendTestRequest");
        unordered_map<std::string, std::string> headers{{"Content-Type", "application/json"}};
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

        co_return;
    }

    Task<void> sendRegexRequest() {
        ICHOR_LOG_INFO(_logger, "sendRegexRequest");
        auto response = co_await _connectionService->sendAsync(HttpMethod::get, "/regex_test/one?param=123", {}, {});
        ICHOR_LOG_ERROR(_logger, "Received status {}", (int)response.status);

        co_return;
    }

    ILogger *_logger{};
    ISerializer<TestMsg> *_serializer{};
    IHttpConnectionService *_connectionService{};
    std::vector<HttpRouteRegistration> _routeRegistrations{};
};
