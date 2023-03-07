#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/TimerService.h>
#include <ichor/services/network/NetworkEvents.h>
#include <ichor/services/network/http/IHttpConnectionService.h>
#include <ichor/services/network/http/IHttpService.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/serialization/ISerializer.h>
#include "../examples/common/TestMsg.h"

using namespace Ichor;

extern std::unique_ptr<Ichor::AsyncManualResetEvent> _evt;
extern std::atomic<bool> evtGate;
extern std::thread::id testThreadId;
extern std::thread::id dmThreadId;

class HttpThreadService final : public AdvancedService<HttpThreadService> {
public:
    HttpThreadService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ISerializer<TestMsg>>(this, true);
        reg.registerDependency<IHttpConnectionService>(this, true, getProperties());
        reg.registerDependency<IHttpService>(this, true);
    }
    ~HttpThreadService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        GetThreadLocalEventQueue().pushEvent<RunFunctionEventAsync>(getServiceId(), [this](DependencyManager &dm) -> AsyncGenerator<IchorBehaviour> {
            auto toSendMsg = _serializer->serialize(TestMsg{11, "hello"});

            if(dmThreadId != std::this_thread::get_id()) {
                throw std::runtime_error("dmThreadId id incorrect");
            }
            if(testThreadId == std::this_thread::get_id()) {
                throw std::runtime_error("testThreadId id incorrect");
            }

            co_await sendTestRequest(std::move(toSendMsg)).begin();

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
        _routeRegistration.reset();
        co_return;
    }

    void addDependencyInstance(ISerializer<TestMsg> &serializer, IService &) {
        _serializer = &serializer;
    }

    void removeDependencyInstance(ISerializer<TestMsg> &serializer, IService&) {
        _serializer = nullptr;
    }

    void addDependencyInstance(IHttpConnectionService &connectionService, IService&) {
        _connectionService = &connectionService;
    }

    void addDependencyInstance(IHttpService &svc, IService&) {
        _routeRegistration = svc.addRoute(HttpMethod::post, "/test", [this](HttpRequest &req) -> AsyncGenerator<HttpResponse> {
            if(dmThreadId != std::this_thread::get_id()) {
                throw std::runtime_error("dmThreadId id incorrect");
            }
            if(testThreadId == std::this_thread::get_id()) {
                throw std::runtime_error("testThreadId id incorrect");
            }

            auto msg = _serializer->deserialize(std::move(req.body));
            evtGate = true;

            co_await *_evt;

            if(dmThreadId != std::this_thread::get_id()) {
                throw std::runtime_error("dmThreadId id incorrect");
            }
            if(testThreadId == std::this_thread::get_id()) {
                throw std::runtime_error("testThreadId id incorrect");
            }

            co_return HttpResponse{false, HttpStatus::ok, "application/json", _serializer->serialize(TestMsg{11, "hello"}), {}};
        });
    }

    void removeDependencyInstance(IHttpService&, IService&) {
        _routeRegistration.reset();
    }

    void removeDependencyInstance(IHttpConnectionService&, IService&) {
    }

    friend DependencyRegister;

    AsyncGenerator<void> sendTestRequest(std::vector<uint8_t> &&toSendMsg) {
        std::vector<HttpHeader> headers{HttpHeader("Content-Type", "application/json")};
        auto response = co_await _connectionService->sendAsync(HttpMethod::post, "/test", std::move(headers), std::move(toSendMsg));

        if(_serializer == nullptr) {
            // we're stopping, gotta bail.
            co_return;
        }

        if(response.status == HttpStatus::ok) {
            auto msg = _serializer->deserialize(std::move(response.body));
        } else {
            throw std::runtime_error("Status not ok");
        }
        GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());

        co_return;
    }

    ISerializer<TestMsg> *_serializer{nullptr};
    IHttpConnectionService *_connectionService{nullptr};
    std::unique_ptr<HttpRouteRegistration> _routeRegistration{nullptr};
};
