#include "Common.h"
#include <ichor/dependency_management/InternalService.h>
#include <ichor/services/network/http/HttpConnectionService.h>

#include "FakeLifecycleManager.h"
// #include "Mocks/ServiceMock.h"
#include "Mocks/QueueMock.h"
#include "Mocks/LoggerMock.h"
#include "Mocks/ConnectionServiceMock.h"

TEST_CASE("HttpTests") {

    Properties props{};
    props.emplace("Address", Ichor::make_any<std::string>("192.168.10.10"));
    props.emplace("Port", Ichor::make_any<std::string>("8080"));
    Detail::DependencyLifecycleManager<QueueMock, IEventQueue> q{{}};
    Detail::DependencyLifecycleManager<LoggerMock, ILogger> logger{{}};
    Detail::DependencyLifecycleManager<ConnectionServiceMock<IClientConnectionService>, IClientConnectionService> conn{{}};
    Detail::DependencyLifecycleManager<HttpConnectionService, IHttpConnectionService> svc{std::move(props)};

    conn.getService().is_client = true;

    auto ret = svc.dependencyOnline(&q);
    REQUIRE(ret == StartBehaviour::DONE);
    ret = svc.dependencyOnline(&logger);
    REQUIRE(ret == StartBehaviour::DONE);
    ret = svc.dependencyOnline(&conn);
    REQUIRE(ret == StartBehaviour::STARTED);

    auto gen = svc.start();
    auto it = gen.begin();
    REQUIRE(it.get_finished());
    REQUIRE(q.getService().events.empty());
    REQUIRE(logger.getService().logs.empty());
    REQUIRE(conn.getService().rcvHandler);

    SECTION("GET basic") {
        unordered_map<std::string, std::string> headers;
        headers.emplace("testheader", "test");
        auto f = [&]() mutable -> AsyncGenerator<tl::expected<HttpResponse, HttpError>> {
            co_return co_await svc.getService().sendAsync(HttpMethod::get, "/some/route", std::move(headers), {});
        };
        auto gen2 = f();
        auto it2 = gen2.begin();
        REQUIRE(!it2.get_finished());
        REQUIRE(conn.getService().sentMessages.size() == 1);
        std::string_view sentMsg{reinterpret_cast<const char *>(conn.getService().sentMessages[0].data()), conn.getService().sentMessages[0].size()};
        REQUIRE(sentMsg == "GET /some/route HTTP/1.1\r\ntestheader: test\r\nHost: 192.168.10.10\r\n\r\n");

        std::string req{"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"};
        conn.getService().rcvHandler(std::span<uint8_t const>{reinterpret_cast<uint8_t*>(req.data()), req.size()});

        REQUIRE(it2.get_finished());
        auto resp = it2.get_value<tl::expected<HttpResponse, HttpError>>();
        REQUIRE(resp);
        REQUIRE(resp->headers.size() == 1);
        REQUIRE(resp->headers["Content-Type"] == "application/json");
        REQUIRE(resp->status == HttpStatus::ok);
        REQUIRE(resp->body.size() == 1);
        REQUIRE(resp->body[0] == 0);
    }

    SECTION("Bad HTTP version response") {
        unordered_map<std::string, std::string> headers;
        headers.emplace("testheader", "test");
        auto f = [&]() mutable -> AsyncGenerator<tl::expected<HttpResponse, HttpError>> {
            co_return co_await svc.getService().sendAsync(HttpMethod::get, "/some/route", std::move(headers), {});
        };
        auto gen2 = f();
        auto it2 = gen2.begin();
        REQUIRE(!it2.get_finished());
        REQUIRE(conn.getService().sentMessages.size() == 1);
        std::string_view sentMsg{reinterpret_cast<const char *>(conn.getService().sentMessages[0].data()), conn.getService().sentMessages[0].size()};
        REQUIRE(sentMsg == "GET /some/route HTTP/1.1\r\ntestheader: test\r\nHost: 192.168.10.10\r\n\r\n");

        std::string req{"HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n\r\n"};
        conn.getService().rcvHandler(std::span<uint8_t const>{reinterpret_cast<uint8_t*>(req.data()), req.size()});

        REQUIRE(it2.get_finished());
        auto resp = it2.get_value<tl::expected<HttpResponse, HttpError>>();
        REQUIRE(!resp);
        REQUIRE(resp.error() == HttpError::UNABLE_TO_PARSE_RESPONSE);
    }

    SECTION("Empty HTTP response") {
        unordered_map<std::string, std::string> headers;
        headers.emplace("testheader", "test");
        auto f = [&]() mutable -> AsyncGenerator<tl::expected<HttpResponse, HttpError>> {
            co_return co_await svc.getService().sendAsync(HttpMethod::get, "/some/route", std::move(headers), {});
        };
        auto gen2 = f();
        auto it2 = gen2.begin();
        REQUIRE(!it2.get_finished());
        REQUIRE(conn.getService().sentMessages.size() == 1);
        std::string_view sentMsg{reinterpret_cast<const char *>(conn.getService().sentMessages[0].data()), conn.getService().sentMessages[0].size()};
        REQUIRE(sentMsg == "GET /some/route HTTP/1.1\r\ntestheader: test\r\nHost: 192.168.10.10\r\n\r\n");

        std::string req{"\r\n\r\n"};
        conn.getService().rcvHandler(std::span<uint8_t const>{reinterpret_cast<uint8_t*>(req.data()), req.size()});

        REQUIRE(it2.get_finished());
        auto resp = it2.get_value<tl::expected<HttpResponse, HttpError>>();
        REQUIRE(!resp);
        REQUIRE(resp.error() == HttpError::UNABLE_TO_PARSE_RESPONSE);
    }

    SECTION("bad send request") {
        unordered_map<std::string, std::string> headers;
        headers.emplace("", "");
        auto f = [&]() mutable -> AsyncGenerator<tl::expected<HttpResponse, HttpError>> {
            co_return co_await svc.getService().sendAsync(HttpMethod::get, "/some/route", std::move(headers), {});
        };
        auto gen2 = f();
        auto it2 = gen2.begin();
        REQUIRE(it2.get_finished());
        auto resp = it2.get_value<tl::expected<HttpResponse, HttpError>>();
        REQUIRE(!resp);
        REQUIRE(resp.error() == HttpError::UNABLE_TO_PARSE_HEADER);
    }
}
