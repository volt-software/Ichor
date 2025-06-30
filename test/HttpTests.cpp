#include "Common.h"
#include <ichor/dependency_management/InternalService.h>
#include <ichor/services/network/http/HttpConnectionService.h>

#include "FakeLifecycleManager.h"
// #include "Mocks/ServiceMock.h"
#include <ichor/services/network/http/HttpHostService.h>

#include "ichor/events/RunFunctionEvent.h"
#include "Mocks/QueueMock.h"
#include "Mocks/LoggerMock.h"
#include "Mocks/ConnectionServiceMock.h"
#include "Mocks/HostServiceMock.h"

TEST_CASE("HttpConnectionTests") {

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
    // REQUIRE(logger.getService().logs.empty());
    REQUIRE(conn.getService().rcvHandler);

    SECTION("GET basic") {
        auto f = [&]() mutable -> AsyncGenerator<tl::expected<HttpResponse, HttpError>> {
            co_return co_await svc.getService().sendAsync(HttpMethod::get, "/some/route", {}, {});
        };
        auto gen2 = f();
        auto it2 = gen2.begin();
        REQUIRE(!it2.get_finished());
        REQUIRE(conn.getService().sentMessages.size() == 1);
        std::string_view sentMsg{reinterpret_cast<const char *>(conn.getService().sentMessages[0].data()), conn.getService().sentMessages[0].size()};
        REQUIRE(sentMsg == "GET /some/route HTTP/1.1\r\nHost: 192.168.10.10\r\n\r\n");

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

    SECTION("GET basic in multiple receives") {
        auto f = [&]() mutable -> AsyncGenerator<tl::expected<HttpResponse, HttpError>> {
            unordered_map<std::string, std::string> headers;
            headers.emplace("testheader", "test");
            co_return co_await svc.getService().sendAsync(HttpMethod::get, "/some/route", std::move(headers), {});
        };
        auto gen2 = f();
        auto it2 = gen2.begin();
        REQUIRE(!it2.get_finished());
        REQUIRE(conn.getService().sentMessages.size() == 1);
        std::string_view sentMsg{reinterpret_cast<const char *>(conn.getService().sentMessages[0].data()), conn.getService().sentMessages[0].size()};
        REQUIRE(sentMsg == "GET /some/route HTTP/1.1\r\ntestheader: test\r\nHost: 192.168.10.10\r\n\r\n");

        std::string req{"HTTP/1.1 200 OK\r\n"};
        conn.getService().rcvHandler(std::span<uint8_t const>{reinterpret_cast<uint8_t*>(req.data()), req.size()});

        REQUIRE(!it2.get_finished());
        req = "Content-Type: application/json\r\n\r\n";
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

    SECTION("GET multiple in buffer") {
        auto f = [&]() mutable -> AsyncGenerator<tl::expected<HttpResponse, HttpError>> {
            unordered_map<std::string, std::string> headers;
            headers.emplace("testheader", "test");
            co_return co_await svc.getService().sendAsync(HttpMethod::get, "/some/route", std::move(headers), {});
        };
        auto gen2 = f();
        auto it2 = gen2.begin();
        REQUIRE(!it2.get_finished());
        REQUIRE(conn.getService().sentMessages.size() == 1);
        auto gen3 = f();
        auto it3 = gen3.begin();
        REQUIRE(!it3.get_finished());
        REQUIRE(conn.getService().sentMessages.size() == 2);
        std::string_view sentMsg{reinterpret_cast<const char *>(conn.getService().sentMessages[0].data()), conn.getService().sentMessages[0].size()};
        REQUIRE(sentMsg == "GET /some/route HTTP/1.1\r\ntestheader: test\r\nHost: 192.168.10.10\r\n\r\n");
        sentMsg = std::string_view{reinterpret_cast<const char *>(conn.getService().sentMessages[1].data()), conn.getService().sentMessages[1].size()};
        REQUIRE(sentMsg == "GET /some/route HTTP/1.1\r\ntestheader: test\r\nHost: 192.168.10.10\r\n\r\n");

        std::string req{"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\nHTTP/1.1 200 OK\r\nContent-Type: application/json2\r\n\r\n"};
        conn.getService().rcvHandler(std::span<uint8_t const>{reinterpret_cast<uint8_t*>(req.data()), req.size()});

        REQUIRE(it2.get_finished());
        auto resp = it2.get_value<tl::expected<HttpResponse, HttpError>>();
        REQUIRE(resp);
        REQUIRE(resp->headers.size() == 1);
        REQUIRE(resp->headers["Content-Type"] == "application/json");
        REQUIRE(resp->status == HttpStatus::ok);
        REQUIRE(resp->body.size() == 1);
        REQUIRE(resp->body[0] == 0);

        REQUIRE(it3.get_finished());
        resp = it3.get_value<tl::expected<HttpResponse, HttpError>>();
        REQUIRE(resp);
        REQUIRE(resp->headers.size() == 1);
        REQUIRE(resp->headers["Content-Type"] == "application/json2");
        REQUIRE(resp->status == HttpStatus::ok);
        REQUIRE(resp->body.size() == 1);
        REQUIRE(resp->body[0] == 0);
    }

    SECTION("POST request with body") {
        auto f = [&]() mutable -> AsyncGenerator<tl::expected<HttpResponse, HttpError>> {
            unordered_map<std::string, std::string> headers;
            headers.emplace("testheader", "test");
            std::string_view body_view = "<html><body>This is my basic webpa\r\n</body></html>";
            std::vector<uint8_t> body{body_view.begin(), body_view.end()};
            co_return co_await svc.getService().sendAsync(HttpMethod::post, "/some/route", std::move(headers), std::move(body));
        };
        auto gen2 = f();
        auto it2 = gen2.begin();
        REQUIRE(!it2.get_finished());
        REQUIRE(conn.getService().sentMessages.size() == 1);
        std::string_view sentMsg{reinterpret_cast<const char *>(conn.getService().sentMessages[0].data()), conn.getService().sentMessages[0].size()};
        REQUIRE(sentMsg == "POST /some/route HTTP/1.1\r\ntestheader: test\r\nHost: 192.168.10.10\r\nContent-Length: 50\r\n\r\n<html><body>This is my basic webpa\r\n</body></html>");

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

    SECTION("GET response with body") {
        auto f = [&]() mutable -> AsyncGenerator<tl::expected<HttpResponse, HttpError>> {
            co_return co_await svc.getService().sendAsync(HttpMethod::get, "/some/route", {}, {});
        };
        auto gen2 = f();
        auto it2 = gen2.begin();
        REQUIRE(!it2.get_finished());
        REQUIRE(conn.getService().sentMessages.size() == 1);
        std::string_view sentMsg{reinterpret_cast<const char *>(conn.getService().sentMessages[0].data()), conn.getService().sentMessages[0].size()};
        REQUIRE(sentMsg == "GET /some/route HTTP/1.1\r\nHost: 192.168.10.10\r\n\r\n");

        std::string req{"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 50\r\n\r\n<html><body>This is my basic webpa\r\n</body></html>"};
        conn.getService().rcvHandler(std::span<uint8_t const>{reinterpret_cast<uint8_t*>(req.data()), req.size()});

        REQUIRE(it2.get_finished());
        auto resp = it2.get_value<tl::expected<HttpResponse, HttpError>>();
        REQUIRE(resp);
        REQUIRE(resp->headers.size() == 2);
        REQUIRE(resp->headers["Content-Type"] == "application/json");
        REQUIRE(resp->headers["Content-Length"] == "50");
        REQUIRE(resp->status == HttpStatus::ok);
        REQUIRE(resp->body.size() == 51);
        std::string_view resBody{reinterpret_cast<const char *>(resp->body.data()), resp->body.size() - 1};
        REQUIRE(resBody == "<html><body>This is my basic webpa\r\n</body></html>");
    }

    SECTION("GET response with body in two packets") {
        auto f = [&]() mutable -> AsyncGenerator<tl::expected<HttpResponse, HttpError>> {
            co_return co_await svc.getService().sendAsync(HttpMethod::get, "/some/route", {}, {});
        };
        auto gen2 = f();
        auto it2 = gen2.begin();
        REQUIRE(!it2.get_finished());
        REQUIRE(conn.getService().sentMessages.size() == 1);
        std::string_view sentMsg{reinterpret_cast<const char *>(conn.getService().sentMessages[0].data()), conn.getService().sentMessages[0].size()};
        REQUIRE(sentMsg == "GET /some/route HTTP/1.1\r\nHost: 192.168.10.10\r\n\r\n");

        std::string req{"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 50\r\n\r\n<html><body>This is"};
        conn.getService().rcvHandler(std::span<uint8_t const>{reinterpret_cast<uint8_t*>(req.data()), req.size()});
        req = " my basic webpage</body></html>";
        conn.getService().rcvHandler(std::span<uint8_t const>{reinterpret_cast<uint8_t*>(req.data()), req.size()});

        REQUIRE(it2.get_finished());
        auto resp = it2.get_value<tl::expected<HttpResponse, HttpError>>();
        REQUIRE(resp);
        REQUIRE(resp->headers.size() == 2);
        REQUIRE(resp->headers["Content-Type"] == "application/json");
        REQUIRE(resp->headers["Content-Length"] == "50");
        REQUIRE(resp->status == HttpStatus::ok);
        REQUIRE(resp->body.size() == 51);
        std::string_view resBody{reinterpret_cast<const char *>(resp->body.data()), resp->body.size() - 1};
        REQUIRE(resBody == "<html><body>This is my basic webpage</body></html>");
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

    SECTION("Bad GET response with body missing content-length") {
        auto f = [&]() mutable -> AsyncGenerator<tl::expected<HttpResponse, HttpError>> {
            co_return co_await svc.getService().sendAsync(HttpMethod::get, "/some/route", {}, {});
        };
        auto gen2 = f();
        auto it2 = gen2.begin();
        REQUIRE(!it2.get_finished());
        REQUIRE(conn.getService().sentMessages.size() == 1);
        std::string_view sentMsg{reinterpret_cast<const char *>(conn.getService().sentMessages[0].data()), conn.getService().sentMessages[0].size()};
        REQUIRE(sentMsg == "GET /some/route HTTP/1.1\r\nHost: 192.168.10.10\r\n\r\n");

        std::string req{"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n<html><body>This is my basic webpage</body></html>"};
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

    SECTION("Chunked response basic") {
        auto f = [&]() mutable -> AsyncGenerator<tl::expected<HttpResponse, HttpError>> {
            co_return co_await svc.getService().sendAsync(HttpMethod::get, "/some/route", {}, {});
        };
        auto gen2 = f();
        auto it2 = gen2.begin();
        REQUIRE(!it2.get_finished());
        REQUIRE(conn.getService().sentMessages.size() == 1);
        std::string_view sentMsg{reinterpret_cast<const char *>(conn.getService().sentMessages[0].data()), conn.getService().sentMessages[0].size()};
        REQUIRE(sentMsg == "GET /some/route HTTP/1.1\r\nHost: 192.168.10.10\r\n\r\n");

        std::string req{"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\n\r\n1a\r\nabcdefghijklmnopqrstuvwxyz\r\n10\r\n1234567890abcdef\r\n0\r\nsome-footer: some-value\r\nanother-footer: another-value\r\n\r\n"};
        conn.getService().rcvHandler(std::span<uint8_t const>{reinterpret_cast<uint8_t*>(req.data()), req.size()});

        REQUIRE(it2.get_finished());
        auto resp = it2.get_value<tl::expected<HttpResponse, HttpError>>();
        REQUIRE(resp);
        REQUIRE(resp->headers.size() == 4);
        REQUIRE(resp->headers["Content-Type"] == "application/json");
        REQUIRE(resp->headers["Transfer-Encoding"] == "chunked");
        REQUIRE(resp->headers["some-footer"] == "some-value");
        REQUIRE(resp->headers["another-footer"] == "another-value");
        REQUIRE(resp->status == HttpStatus::ok);
        REQUIRE(resp->body.size() == 43);
        std::string_view resBody{reinterpret_cast<const char *>(resp->body.data()), resp->body.size() - 1};
        REQUIRE(resBody == "abcdefghijklmnopqrstuvwxyz1234567890abcdef");
    }
}

TEST_CASE("HttpHostTests Missing properties") {

    Properties props{};
    props.emplace("Address", Ichor::make_any<std::string>("192.168.10.10"));
    props.emplace("Port", Ichor::make_any<std::string>("8080"));
    Detail::DependencyLifecycleManager<QueueMock, IEventQueue> q{{}};
    Detail::DependencyLifecycleManager<LoggerMock, ILogger> logger{{}};
    Detail::DependencyLifecycleManager<ConnectionServiceMock<IHostConnectionService>, IHostConnectionService> conn{{}};
    Detail::DependencyLifecycleManager<HostServiceMock, IHostService> host{Properties{props}};
    Detail::DependencyLifecycleManager<HttpHostService, IHttpHostService> svc{std::move(props)};

    conn.getService().is_client = false;

    auto ret = svc.dependencyOnline(&q);
    REQUIRE(ret == StartBehaviour::DONE);
    ret = svc.dependencyOnline(&logger);
    REQUIRE(ret == StartBehaviour::DONE);
    ret = svc.dependencyOnline(&host);
    REQUIRE(ret == StartBehaviour::STARTED);
    ret = svc.dependencyOnline(&conn);
    REQUIRE(ret == StartBehaviour::STARTED);

    auto gen = svc.start();
    auto it = gen.begin();
    REQUIRE(it.get_finished());
    REQUIRE(q.getService().events.empty());
    REQUIRE(!conn.getService().rcvHandler);
}

TEST_CASE("HttpHostTests") {

    Properties props{};
    props.emplace("Address", Ichor::make_any<std::string>("192.168.10.10"));
    props.emplace("Port", Ichor::make_any<std::string>("8080"));
    Detail::DependencyLifecycleManager<QueueMock, IEventQueue> q{{}};
    Detail::DependencyLifecycleManager<LoggerMock, ILogger> logger{{}};
    Detail::DependencyLifecycleManager<HostServiceMock, IHostService> host{Properties{props}};
    Detail::DependencyLifecycleManager<ConnectionServiceMock<IHostConnectionService>, IHostConnectionService> conn{{{"TcpHostService", Ichor::make_any<ServiceIdType>(host.getService().getServiceId())}}};
    Detail::DependencyLifecycleManager<HttpHostService, IHttpHostService> svc{std::move(props)};

    conn.getService().is_client = false;

    auto ret = svc.dependencyOnline(&q);
    REQUIRE(ret == StartBehaviour::DONE);
    ret = svc.dependencyOnline(&logger);
    REQUIRE(ret == StartBehaviour::DONE);
    ret = svc.dependencyOnline(&host);
    REQUIRE(ret == StartBehaviour::STARTED);
    ret = svc.dependencyOnline(&conn);
    REQUIRE(ret == StartBehaviour::STARTED);

    auto gen = svc.start();
    auto it = gen.begin();
    REQUIRE(it.get_finished());
    REQUIRE(q.getService().events.empty());
    REQUIRE(conn.getService().rcvHandler);

    SECTION("GET basic") {
        HttpRequest reqCopy{};
        std::string address{};
        auto reg = svc.getService().addRoute(HttpMethod::get, "/some/route", [&reqCopy, &address](HttpRequest &req) -> Task<HttpResponse> {
            reqCopy = req;
            address = req.address;
            co_return HttpResponse{HttpStatus::ok, "text/plain", {}, {}};
        });
        std::string req{"GET /some/route HTTP/1.1\r\ntestheader: test\r\nHost: 192.168.10.10\r\n\r\n"};
        conn.getService().rcvHandler(std::span<uint8_t const>{reinterpret_cast<uint8_t*>(req.data()), req.size()});
        REQUIRE(q.getService().events.size() == 1);
        REQUIRE(q.getService().events[0]->get_type() == RunFunctionEventAsync::TYPE);
        auto *evt = q.getService().events[0].get();
        auto gen2 = static_cast<RunFunctionEventAsync*>(evt)->fun();
        auto it2 = gen2.begin();
        REQUIRE(it2.get_finished());
        auto _ = it2.get_value<IchorBehaviour>();
        REQUIRE(conn.getService().sentMessages.size() == 1);
        std::string_view sentMsg{reinterpret_cast<const char *>(conn.getService().sentMessages[0].data()), conn.getService().sentMessages[0].size()};
        REQUIRE(sentMsg == "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");

        REQUIRE(reqCopy.headers.size() == 2);
        auto testheader = reqCopy.headers.find("testheader");
        REQUIRE(testheader != reqCopy.headers.end());
        REQUIRE(testheader->second == "test");
        auto hostheader = reqCopy.headers.find("Host");
        REQUIRE(hostheader != reqCopy.headers.end());
        REQUIRE(hostheader->second == "192.168.10.10");

        REQUIRE(reqCopy.route == "/some/route");
        REQUIRE(reqCopy.method == HttpMethod::get);
        REQUIRE(address == "");
        REQUIRE(reqCopy.body.size() == 1);
        REQUIRE(reqCopy.body[0] == '\0');
    }

    SECTION("GET host basic in multiple receives") {
        HttpRequest reqCopy{};
        std::string address{};
        auto reg = svc.getService().addRoute(HttpMethod::get, "/some/route", [&reqCopy, &address](HttpRequest &req) -> Task<HttpResponse> {
            reqCopy = req;
            address = req.address;
            co_return HttpResponse{HttpStatus::ok, "text/plain", {}, {}};
        });
        std::string req{"GET /some/route HTTP/1.1\r\ntestheader: test\r\n"};
        conn.getService().rcvHandler(std::span<uint8_t const>{reinterpret_cast<uint8_t*>(req.data()), req.size()});
        REQUIRE(q.getService().events.size() == 1);
        REQUIRE(q.getService().events[0]->get_type() == RunFunctionEventAsync::TYPE);
        {
            auto *evt = q.getService().events[0].get();
            auto gen2 = static_cast<RunFunctionEventAsync*>(evt)->fun();
            auto it2 = gen2.begin();
            REQUIRE(it2.get_finished());
            auto _ = it2.get_value<IchorBehaviour>();
            REQUIRE(conn.getService().sentMessages.empty());
        }
        req = "Host: 192.168.10.10\r\n\r\n";
        conn.getService().rcvHandler(std::span<uint8_t const>{reinterpret_cast<uint8_t*>(req.data()), req.size()});
        REQUIRE(q.getService().events.size() == 2);
        REQUIRE(q.getService().events[1]->get_type() == RunFunctionEventAsync::TYPE);
        auto *evt = q.getService().events[1].get();
        auto gen2 = static_cast<RunFunctionEventAsync*>(evt)->fun();
        auto it2 = gen2.begin();
        REQUIRE(it2.get_finished());
        auto _ = it2.get_value<IchorBehaviour>();
        REQUIRE(conn.getService().sentMessages.size() == 1);
        std::string_view sentMsg{reinterpret_cast<const char *>(conn.getService().sentMessages[0].data()), conn.getService().sentMessages[0].size()};
        REQUIRE(sentMsg == "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");

        REQUIRE(reqCopy.headers.size() == 2);
        auto testheader = reqCopy.headers.find("testheader");
        REQUIRE(testheader != reqCopy.headers.end());
        REQUIRE(testheader->second == "test");
        auto hostheader = reqCopy.headers.find("Host");
        REQUIRE(hostheader != reqCopy.headers.end());
        REQUIRE(hostheader->second == "192.168.10.10");

        REQUIRE(reqCopy.route == "/some/route");
        REQUIRE(reqCopy.method == HttpMethod::get);
        REQUIRE(address == "");
        REQUIRE(reqCopy.body.size() == 1);
        REQUIRE(reqCopy.body[0] == '\0');
    }

    SECTION("GET basic multiple in buffer") {
        HttpRequest reqCopy{};
        HttpRequest reqCopy2{};
        std::string address{};
        std::string address2{};
        auto reg = svc.getService().addRoute(HttpMethod::get, "/some/route", [&reqCopy, &address](HttpRequest &req) -> Task<HttpResponse> {
            reqCopy = req;
            address = req.address;
            co_return HttpResponse{HttpStatus::ok, "text/plain", {}, {}};
        });
        auto reg2 = svc.getService().addRoute(HttpMethod::get, "/some/route2", [&reqCopy2, &address2](HttpRequest &req) -> Task<HttpResponse> {
            reqCopy2 = req;
            address2 = req.address;
            co_return HttpResponse{HttpStatus::ok, "text/plain", {}, {}};
        });
        std::string req{"GET /some/route HTTP/1.1\r\ntestheader: test\r\nHost: 192.168.10.10\r\n\r\nGET /some/route2 HTTP/1.1\r\ntestheader: test2\r\nHost: 192.168.10.11\r\n\r\n"};
        conn.getService().rcvHandler(std::span<uint8_t const>{reinterpret_cast<uint8_t*>(req.data()), req.size()});
        REQUIRE(q.getService().events.size() == 1);
        REQUIRE(q.getService().events[0]->get_type() == RunFunctionEventAsync::TYPE);
        auto *evt = q.getService().events[0].get();
        auto gen2 = static_cast<RunFunctionEventAsync*>(evt)->fun();
        auto it2 = gen2.begin();
        REQUIRE(it2.get_finished());
        auto _ = it2.get_value<IchorBehaviour>();
        REQUIRE(conn.getService().sentMessages.size() == 2);
        {
            std::string_view sentMsg{reinterpret_cast<const char *>(conn.getService().sentMessages[0].data()), conn.getService().sentMessages[0].size()};
            REQUIRE(sentMsg == "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");

            REQUIRE(reqCopy.headers.size() == 2);
            auto testheader = reqCopy.headers.find("testheader");
            REQUIRE(testheader != reqCopy.headers.end());
            REQUIRE(testheader->second == "test");
            auto hostheader = reqCopy.headers.find("Host");
            REQUIRE(hostheader != reqCopy.headers.end());
            REQUIRE(hostheader->second == "192.168.10.10");

            REQUIRE(reqCopy.route == "/some/route");
            REQUIRE(reqCopy.method == HttpMethod::get);
            REQUIRE(address == "");
            REQUIRE(reqCopy.body.size() == 1);
            REQUIRE(reqCopy.body[0] == '\0');
        }
        {
            std::string_view sentMsg{reinterpret_cast<const char *>(conn.getService().sentMessages[1].data()), conn.getService().sentMessages[1].size()};
            REQUIRE(sentMsg == "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");

            REQUIRE(reqCopy2.headers.size() == 2);
            auto testheader = reqCopy2.headers.find("testheader");
            REQUIRE(testheader != reqCopy2.headers.end());
            REQUIRE(testheader->second == "test2");
            auto hostheader = reqCopy2.headers.find("Host");
            REQUIRE(hostheader != reqCopy2.headers.end());
            REQUIRE(hostheader->second == "192.168.10.11");

            REQUIRE(reqCopy2.route == "/some/route2");
            REQUIRE(reqCopy2.method == HttpMethod::get);
            REQUIRE(address2 == "");
            REQUIRE(reqCopy2.body.size() == 1);
            REQUIRE(reqCopy2.body[0] == '\0');
        }
    }

    SECTION("GET advanced") {
        HttpRequest reqCopy{};
        std::string address{};
        auto reg = svc.getService().addRoute(HttpMethod::get, "/some/route", [&reqCopy, &address](HttpRequest &req) -> Task<HttpResponse> {
            unordered_map<std::string, std::string> resHeaders{{"testres", "buzz"}};
            reqCopy = req;
            address = req.address;
            std::string_view body_view = "<html><body>This is my basic webpage</body></html>";
            std::vector<uint8_t> body{body_view.begin(), body_view.end()};
            co_return HttpResponse{HttpStatus::ok, "text/plain", std::move(body), std::move(resHeaders)};
        });
        std::string req{"GET /some/route HTTP/1.1\r\ntestheader: test\r\nHost: 192.168.10.10\r\n\r\n"};
        conn.getService().rcvHandler(std::span<uint8_t const>{reinterpret_cast<uint8_t*>(req.data()), req.size()});
        REQUIRE(q.getService().events.size() == 1);
        REQUIRE(q.getService().events[0]->get_type() == RunFunctionEventAsync::TYPE);
        auto *evt = q.getService().events[0].get();
        auto gen2 = static_cast<RunFunctionEventAsync*>(evt)->fun();
        auto it2 = gen2.begin();
        REQUIRE(it2.get_finished());
        auto _ = it2.get_value<IchorBehaviour>();
        REQUIRE(conn.getService().sentMessages.size() == 1);
        std::string_view sentMsg{reinterpret_cast<const char *>(conn.getService().sentMessages[0].data()), conn.getService().sentMessages[0].size()};
        REQUIRE(sentMsg == "HTTP/1.1 200 OK\r\ntestres: buzz\r\nContent-Type: text/plain\r\nContent-Length: 50\r\n\r\n<html><body>This is my basic webpage</body></html>");

        REQUIRE(reqCopy.headers.size() == 2);
        auto testheader = reqCopy.headers.find("testheader");
        REQUIRE(testheader != reqCopy.headers.end());
        REQUIRE(testheader->second == "test");
        auto hostheader = reqCopy.headers.find("Host");
        REQUIRE(hostheader != reqCopy.headers.end());
        REQUIRE(hostheader->second == "192.168.10.10");

        REQUIRE(reqCopy.route == "/some/route");
        REQUIRE(reqCopy.method == HttpMethod::get);
        REQUIRE(address == "");
        REQUIRE(reqCopy.body.size() == 1);
        REQUIRE(reqCopy.body[0] == '\0');
    }

    SECTION("POST basic") {
        HttpRequest reqCopy{};
        std::string address{};
        auto reg = svc.getService().addRoute(HttpMethod::post, "/some/route", [&reqCopy, &address](HttpRequest &req) -> Task<HttpResponse> {
            reqCopy = req;
            address = req.address;
            co_return HttpResponse{HttpStatus::ok, "text/plain", {}, {}};
        });
        std::string req{"POST /some/route HTTP/1.1\r\ntestheader: test\r\nHost: 192.168.10.10\r\nContent-Length: 50\r\n\r\n<html><body>This is my basic webpage</body></html>"};
        conn.getService().rcvHandler(std::span<uint8_t const>{reinterpret_cast<uint8_t*>(req.data()), req.size()});
        REQUIRE(q.getService().events.size() == 1);
        REQUIRE(q.getService().events[0]->get_type() == RunFunctionEventAsync::TYPE);
        auto *evt = q.getService().events[0].get();
        auto gen2 = static_cast<RunFunctionEventAsync*>(evt)->fun();
        auto it2 = gen2.begin();
        REQUIRE(it2.get_finished());
        auto _ = it2.get_value<IchorBehaviour>();
        REQUIRE(conn.getService().sentMessages.size() == 1);
        std::string_view sentMsg{reinterpret_cast<const char *>(conn.getService().sentMessages[0].data()), conn.getService().sentMessages[0].size()};
        REQUIRE(sentMsg == "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");

        REQUIRE(reqCopy.headers.size() == 3);
        auto testheader = reqCopy.headers.find("testheader");
        REQUIRE(testheader != reqCopy.headers.end());
        REQUIRE(testheader->second == "test");
        auto contentLengthHeader = reqCopy.headers.find("Content-Length");
        REQUIRE(contentLengthHeader != reqCopy.headers.end());
        REQUIRE(contentLengthHeader->second == "50");
        auto hostheader = reqCopy.headers.find("Host");
        REQUIRE(hostheader != reqCopy.headers.end());
        REQUIRE(hostheader->second == "192.168.10.10");

        REQUIRE(reqCopy.route == "/some/route");
        REQUIRE(reqCopy.method == HttpMethod::post);
        REQUIRE(address == "");
        REQUIRE(reqCopy.body.size() == 51);
        std::string_view reqBody{reinterpret_cast<const char *>(reqCopy.body.data()), reqCopy.body.size() - 1};
        REQUIRE(reqBody == "<html><body>This is my basic webpage</body></html>");
    }

    SECTION("POST body with crlf") {
        HttpRequest reqCopy{};
        std::string address{};
        auto reg = svc.getService().addRoute(HttpMethod::post, "/some/route", [&reqCopy, &address](HttpRequest &req) -> Task<HttpResponse> {
            reqCopy = req;
            address = req.address;
            co_return HttpResponse{HttpStatus::ok, "text/plain", {}, {}};
        });
        std::string req{"POST /some/route HTTP/1.1\r\ntestheader: test\r\nHost: 192.168.10.10\r\nContent-Length: 50\r\n\r\n<html><body>This is my basic webpa\r\n</body></html>"};
        conn.getService().rcvHandler(std::span<uint8_t const>{reinterpret_cast<uint8_t*>(req.data()), req.size()});
        REQUIRE(q.getService().events.size() == 1);
        REQUIRE(q.getService().events[0]->get_type() == RunFunctionEventAsync::TYPE);
        auto *evt = q.getService().events[0].get();
        auto gen2 = static_cast<RunFunctionEventAsync*>(evt)->fun();
        auto it2 = gen2.begin();
        REQUIRE(it2.get_finished());
        auto _ = it2.get_value<IchorBehaviour>();
        REQUIRE(conn.getService().sentMessages.size() == 1);
        std::string_view sentMsg{reinterpret_cast<const char *>(conn.getService().sentMessages[0].data()), conn.getService().sentMessages[0].size()};
        REQUIRE(sentMsg == "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");

        REQUIRE(reqCopy.headers.size() == 3);
        auto testheader = reqCopy.headers.find("testheader");
        REQUIRE(testheader != reqCopy.headers.end());
        REQUIRE(testheader->second == "test");
        auto contentLengthHeader = reqCopy.headers.find("Content-Length");
        REQUIRE(contentLengthHeader != reqCopy.headers.end());
        REQUIRE(contentLengthHeader->second == "50");
        auto hostheader = reqCopy.headers.find("Host");
        REQUIRE(hostheader != reqCopy.headers.end());
        REQUIRE(hostheader->second == "192.168.10.10");

        REQUIRE(reqCopy.route == "/some/route");
        REQUIRE(reqCopy.method == HttpMethod::post);
        REQUIRE(address == "");
        REQUIRE(reqCopy.body.size() == 51);
        std::string_view reqBody{reinterpret_cast<const char *>(reqCopy.body.data()), reqCopy.body.size() - 1};
        REQUIRE(reqBody == "<html><body>This is my basic webpa\r\n</body></html>");
    }

    SECTION("POST basic in two packets") {
        HttpRequest reqCopy{};
        std::string address{};
        auto reg = svc.getService().addRoute(HttpMethod::post, "/some/route", [&reqCopy, &address](HttpRequest &req) -> Task<HttpResponse> {
            reqCopy = req;
            address = req.address;
            co_return HttpResponse{HttpStatus::ok, "text/plain", {}, {}};
        });
        std::string req{"POST /some/route HTTP/1.1\r\ntestheader: test\r\nHost: 192.168.10.10\r\nContent-Length: 50\r\n\r\n<html><body>This"};
        conn.getService().rcvHandler(std::span<uint8_t const>{reinterpret_cast<uint8_t*>(req.data()), req.size()});
        REQUIRE(q.getService().events.size() == 1);
        REQUIRE(q.getService().events[0]->get_type() == RunFunctionEventAsync::TYPE);
        {
            auto *evt = q.getService().events[0].get();
            auto gen2 = static_cast<RunFunctionEventAsync*>(evt)->fun();
            auto it2 = gen2.begin();
            REQUIRE(it2.get_finished());
            auto _ = it2.get_value<IchorBehaviour>();
            REQUIRE(conn.getService().sentMessages.empty());
        }
        req = " is my basic webpage</body></html>";
        conn.getService().rcvHandler(std::span<uint8_t const>{reinterpret_cast<uint8_t*>(req.data()), req.size()});
        REQUIRE(q.getService().events.size() == 2);
        REQUIRE(q.getService().events[0]->get_type() == RunFunctionEventAsync::TYPE);
        REQUIRE(q.getService().events[1]->get_type() == RunFunctionEventAsync::TYPE);
        {
            auto *evt = q.getService().events[1].get();
            auto gen2 = static_cast<RunFunctionEventAsync*>(evt)->fun();
            auto it2 = gen2.begin();
            REQUIRE(it2.get_finished());
            auto _ = it2.get_value<IchorBehaviour>();
            REQUIRE(conn.getService().sentMessages.size() == 1);
            std::string_view sentMsg{reinterpret_cast<const char *>(conn.getService().sentMessages[0].data()), conn.getService().sentMessages[0].size()};
            REQUIRE(sentMsg == "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");

            REQUIRE(reqCopy.headers.size() == 3);
            auto testheader = reqCopy.headers.find("testheader");
            REQUIRE(testheader != reqCopy.headers.end());
            REQUIRE(testheader->second == "test");
            auto contentLengthHeader = reqCopy.headers.find("Content-Length");
            REQUIRE(contentLengthHeader != reqCopy.headers.end());
            REQUIRE(contentLengthHeader->second == "50");
            auto hostheader = reqCopy.headers.find("Host");
            REQUIRE(hostheader != reqCopy.headers.end());
            REQUIRE(hostheader->second == "192.168.10.10");

            REQUIRE(reqCopy.route == "/some/route");
            REQUIRE(reqCopy.method == HttpMethod::post);
            REQUIRE(address == "");
            REQUIRE(reqCopy.body.size() == 51);
            std::string_view reqBody{reinterpret_cast<const char *>(reqCopy.body.data()), reqCopy.body.size() - 1};
            REQUIRE(reqBody == "<html><body>This is my basic webpage</body></html>");
        }
    }

}
