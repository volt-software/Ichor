#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/services/network/http/IHttpConnectionService.h>
#include <ichor/services/network/http/IHttpHostService.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/serialization/ISerializer.h>
#include "../examples/common/TestMsg.h"
#include "../serialization/RegexJsonMsg.h"

using namespace Ichor;

extern std::unique_ptr<Ichor::AsyncManualResetEvent> _evt;
extern std::atomic<bool> evtGate;
extern std::thread::id testThreadId;
extern std::thread::id dmThreadId;

class HttpThreadService final : public AdvancedService<HttpThreadService> {
public:
    HttpThreadService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ISerializer<TestMsg>>(this, DependencyFlags::REQUIRED);
        reg.registerDependency<ISerializer<RegexJsonMsg>>(this, DependencyFlags::REQUIRED);
        reg.registerDependency<IHttpConnectionService>(this, DependencyFlags::REQUIRED, getProperties());
        reg.registerDependency<IHttpHostService>(this, DependencyFlags::REQUIRED);
    }
    ~HttpThreadService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        GetThreadLocalEventQueue().pushEvent<RunFunctionEventAsync>(getServiceId(), [this]() -> AsyncGenerator<IchorBehaviour> {
            auto toSendMsg = _testSerializer->serialize(TestMsg{11, "hello"});

            if(dmThreadId != std::this_thread::get_id()) {
                throw std::runtime_error("dmThreadId id incorrect");
            }
            if(testThreadId == std::this_thread::get_id()) {
                throw std::runtime_error("testThreadId id incorrect");
            }

            co_await sendTestRequest(std::move(toSendMsg));

            if(dmThreadId != std::this_thread::get_id()) {
                throw std::runtime_error("dmThreadId id incorrect");
            }
            if(testThreadId == std::this_thread::get_id()) {
                throw std::runtime_error("testThreadId id incorrect");
            }

            co_return {};
        });

        co_return {};
    }

    Task<void> stop() final {
        _routeRegistration = {};
        _regexRouteRegistration = {};
        co_return;
    }

    void addDependencyInstance(ISerializer<TestMsg> &serializer, IService &) {
        _testSerializer = &serializer;
    }

    void removeDependencyInstance(ISerializer<TestMsg> &, IService&) {
        _testSerializer = nullptr;
    }

    void addDependencyInstance(ISerializer<RegexJsonMsg> &serializer, IService &) {
        _regexSerializer = &serializer;
    }

    void removeDependencyInstance(ISerializer<RegexJsonMsg> &, IService&) {
        _regexSerializer = nullptr;
    }

    void addDependencyInstance(IHttpConnectionService &connectionService, IService&) {
        _connectionService = &connectionService;
    }

    void addDependencyInstance(IHttpHostService &svc, IService&) {
        _routeRegistration = svc.addRoute(HttpMethod::post, "/test", [this](HttpRequest &req) -> Task<HttpResponse> {
            fmt::println("/test POST");
            if(dmThreadId != std::this_thread::get_id()) {
                throw std::runtime_error("dmThreadId id incorrect");
            }
            if(testThreadId == std::this_thread::get_id()) {
                throw std::runtime_error("testThreadId id incorrect");
            }

            auto msg = _testSerializer->deserialize(req.body);
            evtGate = true;

            co_await *_evt;

            if(dmThreadId != std::this_thread::get_id()) {
                throw std::runtime_error("dmThreadId id incorrect");
            }
            if(testThreadId == std::this_thread::get_id()) {
                throw std::runtime_error("testThreadId id incorrect");
            }

            co_return HttpResponse{HttpStatus::ok, "application/json", _testSerializer->serialize(TestMsg{11, "hello"}), {}};
        });
        _regexRouteRegistration = svc.addRoute(HttpMethod::get, std::make_unique<RegexRouteMatch<R"(\/regex_test\/([a-zA-Z0-9]*)\?*([a-zA-Z0-9]+=[a-zA-Z0-9]+)*&*([a-zA-Z0-9]+=[a-zA-Z0-9]+)*)">>(), [this](HttpRequest &req) -> Task<HttpResponse> {
            fmt::println("/regex_test POST");
            if(dmThreadId != std::this_thread::get_id()) {
                throw std::runtime_error("dmThreadId id incorrect");
            }
            if(testThreadId == std::this_thread::get_id()) {
                throw std::runtime_error("testThreadId id incorrect");
            }

            co_return HttpResponse{HttpStatus::ok, "application/json", _regexSerializer->serialize(RegexJsonMsg{std::move(req.regex_params)}), {}};
        });
    }

    void removeDependencyInstance(IHttpHostService&, IService&) {
        _routeRegistration.reset();
        _regexRouteRegistration.reset();
    }

    void removeDependencyInstance(IHttpConnectionService&, IService&) {
    }

    friend DependencyRegister;

    Task<void> sendTestRequest(std::vector<uint8_t> &&toSendMsg) {
        unordered_map<std::string, std::string> headers{{"Content-Type", "application/json"}};
        auto response = co_await _connectionService->sendAsync(HttpMethod::post, "/test", std::move(headers), std::move(toSendMsg));

        if(_testSerializer == nullptr) {
            // we're stopping, gotta bail.
            co_return;
        }

        if(response.status != HttpStatus::ok) {
            fmt::println("test status not ok {}", static_cast<uint_fast16_t>(response.status));
            throw std::runtime_error("test status not ok");
        }

        auto msg = _testSerializer->deserialize(response.body);
        if(!msg) {
            std::terminate();
        }
        fmt::print("Received TestMsg {}:{}\n", msg->id, msg->val);

        GetThreadLocalEventQueue().pushEvent<RunFunctionEventAsync>(getServiceId(), [this]() -> AsyncGenerator<IchorBehaviour> {
            co_await sendRegexRequest();
            co_return {};
        });

        co_return;
    }

    void print_query_params(RegexJsonMsg const &msg) {
        fmt::print("Received RegexJsonMsg {} ", msg.query_params.size());
        for(auto const &param : msg.query_params) {
            fmt::print(" \"{}\"", param);
        }
        fmt::print("\n");
    }

    Task<void> sendRegexRequest() {
        auto response = co_await _connectionService->sendAsync(HttpMethod::get, "/regex_test/one", {}, {});

        if(_regexSerializer == nullptr) {
            // we're stopping, gotta bail.
            co_return;
        }

        if(response.status != HttpStatus::ok) {
            throw std::runtime_error(fmt::format("regex1 status not ok {}", (int)response.status));
        }

        auto msg = _regexSerializer->deserialize(response.body);
        if(!msg) {
            throw std::runtime_error("regex1 could not deserialize");
        }
        print_query_params(msg.value());

        if(msg->query_params.size() != 1) {
            throw std::runtime_error(fmt::format("regex1 query_params size not 1 {}", msg->query_params.size()));
        }
        if(msg->query_params[0] != "one") {
            throw std::runtime_error("regex1 query_params[0] not one");
        }

        response = co_await _connectionService->sendAsync(HttpMethod::get, "/regex_test/two?", {}, {});

        if(_regexSerializer == nullptr) {
            // we're stopping, gotta bail.
            co_return;
        }

        if(response.status != HttpStatus::ok) {
            throw std::runtime_error(fmt::format("regex2 status not ok {}", (int)response.status));
        }

        msg = _regexSerializer->deserialize(response.body);
        if(!msg) {
            throw std::runtime_error("regex2 could not deserialize");
        }
        print_query_params(msg.value());

        if(msg->query_params.size() != 1) {
            throw std::runtime_error("regex2 query_params size not 1");
        }
        if(msg->query_params[0] != "two") {
            throw std::runtime_error("regex2 query_params[0] not two");
        }

        response = co_await _connectionService->sendAsync(HttpMethod::get, "/regex_test/three?param=test", {}, {});

        if(_regexSerializer == nullptr) {
            // we're stopping, gotta bail.
            co_return;
        }

        if(response.status != HttpStatus::ok) {
            throw std::runtime_error(fmt::format("regex3 status not ok {}", (int)response.status));
        }

        msg = _regexSerializer->deserialize(response.body);
        if(!msg) {
            throw std::runtime_error("regex3 could not deserialize");
        }
        print_query_params(msg.value());

        if(msg->query_params.size() != 2) {
            throw std::runtime_error("regex3 query_params size not 2");
        }
        if(msg->query_params[0] != "three") {
            throw std::runtime_error("regex3 query_params[0] not three");
        }
        if(msg->query_params[1] != "param=test") {
            throw std::runtime_error("regex3 query_params[0] not param=test");
        }

        response = co_await _connectionService->sendAsync(HttpMethod::get, "/regex_test/four?param=test&second=123", {}, {});

        if(_regexSerializer == nullptr) {
            // we're stopping, gotta bail.
            co_return;
        }

        if(response.status != HttpStatus::ok) {
            throw std::runtime_error(fmt::format("regex4 status not ok {}", (int)response.status));
        }

        msg = _regexSerializer->deserialize(response.body);
        if(!msg) {
            throw std::runtime_error("regex4 could not deserialize");
        }
        print_query_params(msg.value());

        if(msg->query_params.size() != 3) {
            throw std::runtime_error("regex4 query_params size not 2");
        }
        if(msg->query_params[0] != "four") {
            throw std::runtime_error("regex4 query_params[0] not three");
        }
        if(msg->query_params[1] != "param=test") {
            throw std::runtime_error("regex4 query_params[0] not param=test");
        }
        if(msg->query_params[2] != "second=123") {
            throw std::runtime_error("regex4 query_params[0] not param=test");
        }

        GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());

        co_return;
    }

    ISerializer<TestMsg> *_testSerializer{};
    ISerializer<RegexJsonMsg> *_regexSerializer{};
    IHttpConnectionService *_connectionService{};
    HttpRouteRegistration _routeRegistration{};
    HttpRouteRegistration _regexRouteRegistration{};
};
