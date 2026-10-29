// HttpHostService against RFC 9110 (HTTP semantics) and RFC 9112 (HTTP/1.1), without a DependencyManager: the dependencies are injected by hand.
#include "Common.h"
#include <ichor/services/network/http/HttpHostService.h>
#include <ichor/services/network/http/HttpRequestParser.h>
#include <ichor/dependency_management/InternalServiceLifecycleManager.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include "Mocks/QueueMock.h"
#include "Mocks/LoggerMock.h"
#include "Mocks/ConnectionServiceMock.h"
#include "Mocks/HostServiceMock.h"
#include "Mocks/TimerFactoryMock.h"
#include <list>

using namespace std::literals;

namespace {
    struct Response {
        uint_fast16_t status{};
        std::string reason;
        HttpHeaders headers;
        std::string body;
        /// Everything before the body, without the Date field
        std::string head;
    };

    /// Parses a response sent by HttpHostService. Checks the framing and that it has a Date in the IMF-fixdate format, which head leaves out.
    Response parseResponse(std::string_view msg) {
        Response ret;
        auto const headEnd = msg.find("\r\n\r\n");
        REQUIRE(headEnd != std::string_view::npos);
        auto const head = msg.substr(0, headEnd + 2);
        ret.body = msg.substr(headEnd + 4);

        auto const statusLineEnd = head.find("\r\n");
        auto const statusLine = head.substr(0, statusLineEnd);
        REQUIRE(statusLine.starts_with("HTTP/1.1 "));
        REQUIRE(statusLine.size() >= 13);
        REQUIRE(statusLine[12] == ' ');
        ret.status = static_cast<uint_fast16_t>(std::stoul(std::string{statusLine.substr(9, 3)}));
        ret.reason = statusLine.substr(13);
        ret.head = std::string{statusLine} + "\r\n";

        size_t pos = statusLineEnd + 2;
        while(pos < head.size()) {
            auto const lineEnd = head.find("\r\n", pos);
            auto const line = head.substr(pos, lineEnd - pos);
            pos = lineEnd + 2;
            auto const colon = line.find(": ");
            REQUIRE(colon != std::string_view::npos);
            auto const name = line.substr(0, colon);
            auto const value = line.substr(colon + 2);
            REQUIRE(ret.headers.find(name) == ret.headers.end());
            ret.headers.emplace(std::string{name}, std::string{value});
            if(name == "Date") {
                REQUIRE(value.size() == 29);
                REQUIRE(Detail::parseHttpDate(value));
            } else {
                ret.head += std::string{line} + "\r\n";
            }
        }
        if(ret.status >= 200) {
            REQUIRE(ret.headers.contains("Date"));
        }
        if(auto contentLength = ret.headers.find("Content-Length"); contentLength != ret.headers.end() && ret.status != 304 && ret.body.size() != 0) {
            REQUIRE(contentLength->second == std::to_string(ret.body.size()));
        }
        return ret;
    }

    struct HttpHostFixture {
        explicit HttpHostFixture(Properties extraProps = {}, std::string_view receivedBeforeHandler = {}, bool withTimerFactory = false) {
            Properties props{};
            props.emplace("Address", Ichor::v1::make_any<std::string>("192.168.10.10"));
            props.emplace("Port", Ichor::v1::make_any<std::string>("8080"));
            for(auto &[k, v] : extraProps) {
                props.insert_or_assign(k, std::move(v));
            }
            host = std::make_unique<Ichor::v1::Detail::DependencyLifecycleManager<HostServiceMock, IHostService>>(Properties{props});
            conn = std::make_unique<Ichor::v1::Detail::DependencyLifecycleManager<ConnectionServiceMock<IHostConnectionService>, IHostConnectionService>>(Properties{{"TcpHostService", Ichor::v1::make_any<ServiceIdType>(host->getService().getServiceId())}});
            svc = std::make_unique<Ichor::v1::Detail::DependencyLifecycleManager<HttpHostService, IHttpHostService>>(std::move(props));
            conn->getService().receivedBeforeHandler = receivedBeforeHandler;

            REQUIRE(svc->dependencyOnline(&q) == StartBehaviour::DONE);
            REQUIRE(svc->dependencyOnline(&logger) == StartBehaviour::DONE);
            REQUIRE(svc->dependencyOnline(host.get()) == StartBehaviour::STARTED);
            REQUIRE(svc->dependencyOnline(conn.get()) == StartBehaviour::STARTED);
            if(withTimerFactory) {
                timerFactory = std::make_unique<Ichor::v1::Detail::DependencyLifecycleManager<TimerFactoryMock, ITimerFactory>>(Properties{});
                (void)svc->dependencyOnline(timerFactory.get());
            }
            auto gen = svc->start();
            auto it = gen.begin();
            REQUIRE(it.get_finished());
            REQUIRE(conn->getService().rcvHandler);
        }

        HttpHostService &service() {
            return svc->getService();
        }

        void addRoute(HttpMethod method, std::string_view route, std::function<Task<HttpResponse>(HttpRequest&)> handler) {
            registrations.emplace_back(service().addRoute(method, route, std::move(handler)));
        }

        void addRoute(HttpMethod method, std::unique_ptr<RouteMatcher> matcher, std::function<Task<HttpResponse>(HttpRequest&)> handler) {
            registrations.emplace_back(service().addRoute(method, std::move(matcher), std::move(handler)));
        }

        void addStreamingRoute(HttpMethod method, std::string_view route, HttpStreamingHandler handler) {
            registrations.emplace_back(service().addStreamingRoute(method, route, std::move(handler)));
        }

        /// Fires the timeout timer and runs the events it caused
        void tick() {
            REQUIRE(timerFactory);
            timerFactory->getService().fire();
            runEvents();
        }

        /// Everything sent, as one string
        [[nodiscard]] std::string allSent() const {
            std::string ret;
            for(auto const &msg : sent()) {
                ret += msg;
            }
            return ret;
        }

        /// Receives data on the connection and runs the events it caused
        void receive(std::string_view data) {
            conn->getService().rcvHandler(std::span<uint8_t const>{reinterpret_cast<uint8_t const*>(data.data()), data.size()});
            runEvents();
        }

        void runEvents() {
            while(ranEvents < qm.events.size()) {
                auto *evt = qm.events[ranEvents++].get();
                if(evt->get_type() != RunFunctionEventAsync::TYPE) {
                    continue;
                }
                auto &running = generators.emplace_back(static_cast<RunFunctionEventAsync*>(evt)->fun());
                // a handler that waits on something keeps its generator suspended
                (void)running.begin();
            }
        }

        [[nodiscard]] std::vector<std::string> sent() const {
            std::vector<std::string> ret;
            for(auto const &msg : conn->getService().sentMessages) {
                ret.emplace_back(reinterpret_cast<char const*>(msg.data()), msg.size());
            }
            return ret;
        }

        [[nodiscard]] std::vector<Response> responses() const {
            std::vector<Response> ret;
            for(auto const &msg : sent()) {
                ret.emplace_back(parseResponse(msg));
            }
            return ret;
        }

        /// The only response sent
        [[nodiscard]] Response response() const {
            auto all = responses();
            REQUIRE(all.size() == 1);
            return all[0];
        }

        /// Whether HttpHostService asked for the connection to be stopped, which closes it right away
        [[nodiscard]] bool stopped() const {
            return std::ranges::any_of(qm.events, [this](auto const &evt) {
                return evt->get_type() == StopServiceEvent::TYPE && static_cast<StopServiceEvent const *>(evt.get())->serviceId == conn->getService().getServiceId();
            });
        }

        /// Takes an optional dependency away, like the DependencyManager does when it stops
        void removeDependency(ILifecycleManager &dependency) {
            auto deps = svc->interestedInDependencyGoingOffline(&dependency);
            REQUIRE(!deps.empty());
            auto gen = svc->dependencyOffline(&dependency, deps);
            REQUIRE(gen.begin().get_finished());
            svc->finishDependencyOffline(&dependency, deps);
        }

        /// Whether HttpHostService closed the connection, right away or gracefully (sending side first)
        [[nodiscard]] bool closed() const {
            return stopped() || conn->getService().sendShutdown;
        }

        QueueMock qm{};
        Ichor::v1::Detail::InternalServiceLifecycleManager<IEventQueue> q{&qm};
        Ichor::v1::Detail::DependencyLifecycleManager<LoggerMock, ILogger> logger{{}};
        std::unique_ptr<Ichor::v1::Detail::DependencyLifecycleManager<HostServiceMock, IHostService>> host;
        std::unique_ptr<Ichor::v1::Detail::DependencyLifecycleManager<ConnectionServiceMock<IHostConnectionService>, IHostConnectionService>> conn;
        std::unique_ptr<Ichor::v1::Detail::DependencyLifecycleManager<TimerFactoryMock, ITimerFactory>> timerFactory;
        std::unique_ptr<Ichor::v1::Detail::DependencyLifecycleManager<HttpHostService, IHttpHostService>> svc;
        std::vector<HttpRouteRegistration> registrations;
        std::list<AsyncGenerator<IchorBehaviour>> generators;
        size_t ranEvents{};
    };

    std::vector<uint8_t> toBody(std::string_view str) {
        return {str.begin(), str.end()};
    }

    /// What a handler received. HttpRequest's views are only valid while the handler runs.
    struct RecordedRequest {
        HttpMethod method{HttpMethod::unknown};
        std::string route;
        HttpHeaders headers;
        /// Without the null terminator HttpHostService adds
        std::string body;
    };

    /// A handler that records the request and responds with 200 and body
    auto recordingHandler(RecordedRequest &recorded, std::string body = "ok") {
        return [&recorded, body = std::move(body)](HttpRequest &req) -> Task<HttpResponse> {
            REQUIRE(!req.body.empty());
            REQUIRE(req.body.back() == 0);
            recorded = RecordedRequest{.method = req.method, .route = std::string{req.route}, .headers = req.headers, .body = std::string{req.body.begin(), req.body.end() - 1}};
            co_return HttpResponse{.status = HttpStatus::ok, .contentType = "text/plain", .body = toBody(body), .headers = {}};
        };
    }
}

TEST_CASE("HttpHostRfcTests: HTTP-date") {
    // RFC 9110 5.6.7: 784111777 is Sun, 06 Nov 1994 08:49:37 GMT
    auto const t = std::chrono::system_clock::time_point{std::chrono::seconds{784111777}};
    REQUIRE(Detail::formatHttpDate(t) == "Sun, 06 Nov 1994 08:49:37 GMT");
    REQUIRE(Detail::formatHttpDate(std::chrono::system_clock::time_point{}) == "Thu, 01 Jan 1970 00:00:00 GMT");

    auto const expected = std::chrono::sys_seconds{std::chrono::seconds{784111777}};
    REQUIRE(Detail::parseHttpDate("Sun, 06 Nov 1994 08:49:37 GMT") == expected);
    REQUIRE(Detail::parseHttpDate("Sunday, 06-Nov-94 08:49:37 GMT") == expected);
    REQUIRE(Detail::parseHttpDate("Sun Nov  6 08:49:37 1994") == expected);
    REQUIRE(Detail::parseHttpDate("Sun Nov 16 08:49:37 1994") == expected + std::chrono::days{10});

    REQUIRE(!Detail::parseHttpDate(""));
    REQUIRE(!Detail::parseHttpDate("Sun, 06 Nov 1994 08:49:37 UTC"));
    REQUIRE(!Detail::parseHttpDate("Sun, 31 Nov 1994 08:49:37 GMT"));
    REQUIRE(!Detail::parseHttpDate("Sun, 06 Nov 1994 24:00:00 GMT"));
    REQUIRE(!Detail::parseHttpDate("Sun, 6 Nov 1994 08:49:37 GMT"));
    REQUIRE(!Detail::parseHttpDate("Sunday, 06 Nov 1994 08:49:37 GMT"));
    REQUIRE(!Detail::parseHttpDate("Xyz Nov  6 08:49:37 1994"));
}

TEST_CASE("HttpHostRfcTests: request line") {
    HttpHostFixture f;
    RecordedRequest req{};
    f.addRoute(HttpMethod::get, "/some/route", recordingHandler(req));

    SECTION("basic GET has Date and Content-Length") {
        f.receive("GET /some/route HTTP/1.1\r\nHost: example.com\r\n\r\n");
        auto resp = f.response();
        REQUIRE(resp.head == "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n");
        REQUIRE(resp.body == "ok");
        REQUIRE(req.method == HttpMethod::get);
        REQUIRE(req.route == "/some/route");
        REQUIRE(!f.closed());
    }

    SECTION("empty lines before the request line are ignored") {
        f.receive("\r\n\r\nGET /some/route HTTP/1.1\r\nHost: example.com\r\n\r\n");
        REQUIRE(f.response().status == 200);
    }

    SECTION("an unknown method is 501 and the connection stays open") {
        f.receive("BREW /some/route HTTP/1.1\r\nHost: example.com\r\nContent-Length: 3\r\n\r\nabcGET /some/route HTTP/1.1\r\nHost: example.com\r\n\r\n");
        auto resps = f.responses();
        REQUIRE(resps.size() == 2);
        REQUIRE(resps[0].status == 501);
        REQUIRE(resps[0].reason == "Not Implemented");
        REQUIRE(resps[1].status == 200);
        REQUIRE(!f.closed());
    }

    SECTION("methods are case-sensitive") {
        f.receive("get /some/route HTTP/1.1\r\nHost: example.com\r\n\r\n");
        REQUIRE(f.response().status == 501);
    }

    SECTION("a method that isn't a token is 400") {
        f.receive("G(ET /some/route HTTP/1.1\r\nHost: example.com\r\n\r\n");
        REQUIRE(f.response().status == 400);
        REQUIRE(f.response().headers.find("Connection")->second == "close");
        REQUIRE(f.closed());
    }

    SECTION("more than one space is 400") {
        f.receive("GET  /some/route HTTP/1.1\r\nHost: example.com\r\n\r\n");
        REQUIRE(f.response().status == 400);
        REQUIRE(f.closed());
    }

    SECTION("a missing version is 400") {
        f.receive("GET /some/route\r\nHost: example.com\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("a malformed version is 400") {
        f.receive("GET /some/route http/1.1\r\nHost: example.com\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("major versions other than 1 are 505") {
        f.receive("GET /some/route HTTP/2.0\r\nHost: example.com\r\n\r\n");
        REQUIRE(f.response().status == 505);
        REQUIRE(f.closed());
    }

    SECTION("later minor versions are handled as 1.1") {
        f.receive("GET /some/route HTTP/1.9\r\nHost: example.com\r\n\r\n");
        auto resp = f.response();
        REQUIRE(resp.status == 200);
        REQUIRE(!resp.headers.contains("Connection"));
    }

    SECTION("bare LF line endings are 400") {
        f.receive("GET /some/route HTTP/1.1\nHost: example.com\n\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("a bare CR is 400") {
        f.receive("GET /some/route HTTP/1.1\r\nHost: exa\rmple.com\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("a request line longer than the maximum is 414") {
        f.receive("GET /" + std::string(9000, 'a'));
        REQUIRE(f.response().status == 414);
        REQUIRE(f.closed());
    }

    SECTION("a header section larger than the maximum is 431") {
        f.receive("GET /some/route HTTP/1.1\r\nHost: example.com\r\n");
        for(int i = 0; i < 1000; i++) {
            f.receive("X-Filler: " + std::string(100, 'a') + "\r\n");
        }
        REQUIRE(f.response().status == 431);
        REQUIRE(f.closed());
    }

    SECTION("nothing is sent for an incomplete request") {
        f.receive("GET /some/route HTTP/1.1\r\nHost: exam");
        REQUIRE(f.sent().empty());
        f.receive("ple.com\r\n\r\n");
        REQUIRE(f.response().status == 200);
    }
}

TEST_CASE("HttpHostRfcTests: limits from properties") {
    HttpHostFixture f{Properties{{"MaxRequestLineSize", Ichor::v1::make_any<uint64_t>(30ULL)}, {"MaxHeaderSize", Ichor::v1::make_any<uint64_t>(100ULL)}, {"MaxBodySize", Ichor::v1::make_any<uint64_t>(10ULL)}}};
    RecordedRequest req{};
    f.addRoute(HttpMethod::post, "/r", recordingHandler(req));

    SECTION("request line") {
        f.receive("POST /rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr HTTP/1.1\r\n");
        REQUIRE(f.response().status == 414);
    }

    SECTION("header section") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.com\r\nX-Filler: " + std::string(100, 'a') + "\r\n\r\n");
        REQUIRE(f.response().status == 431);
    }

    SECTION("Content-Length") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.com\r\nContent-Length: 11\r\n\r\n");
        REQUIRE(f.response().status == 413);
        REQUIRE(f.closed());
    }

    SECTION("chunked") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.com\r\nTransfer-Encoding: chunked\r\n\r\n6\r\nabcdef\r\n5\r\nghijk\r\n");
        REQUIRE(f.response().status == 413);
    }

    SECTION("at the limit") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.com\r\nContent-Length: 10\r\n\r\n0123456789");
        REQUIRE(f.response().status == 200);
        REQUIRE(req.body == "0123456789");
    }
}

TEST_CASE("HttpHostRfcTests: request target") {
    HttpHostFixture f;
    RecordedRequest req{};
    f.addRoute(HttpMethod::get, "/some/route?a=b", recordingHandler(req));
    f.addRoute(HttpMethod::get, "/", recordingHandler(req));

    SECTION("absolute-form is turned into origin-form, its authority replaces Host") {
        f.receive("GET http://example.org:8080/some/route?a=b HTTP/1.1\r\nHost: other.example.com\r\n\r\n");
        REQUIRE(f.response().status == 200);
        REQUIRE(req.route == "/some/route?a=b");
        REQUIRE(req.headers.find("Host")->second == "example.org:8080");
    }

    SECTION("absolute-form without a path is /") {
        f.receive("GET HTTP://example.org HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.response().status == 200);
        REQUIRE(req.route == "/");
    }

    SECTION("absolute-form with userinfo is 400") {
        f.receive("GET http://user@example.org/ HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("absolute-form with another scheme is 400") {
        f.receive("GET ftp://example.org/ HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("asterisk-form is only for OPTIONS") {
        f.receive("GET * HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("characters that aren't allowed in a URI are 400") {
        f.receive("GET /some/<route> HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("a fragment is 400") {
        f.receive("GET /some/route#a HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("an invalid percent-encoding is 400") {
        f.receive("GET /some/%zzroute HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("percent-encoding is passed on as is") {
        f.receive("GET /some/%20route HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.response().status == 404);
    }

    SECTION("CONNECT needs authority-form") {
        f.receive("CONNECT /some/route HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("authority-form is only for CONNECT") {
        f.receive("GET example.org:443 HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("CONNECT without a route is 404") {
        f.receive("CONNECT example.org:443 HTTP/1.1\r\nHost: example.org:443\r\n\r\n");
        REQUIRE(f.response().status == 404);
    }
}

TEST_CASE("HttpHostRfcTests: header fields") {
    HttpHostFixture f;
    RecordedRequest req{};
    f.addRoute(HttpMethod::get, "/r", recordingHandler(req));

    SECTION("no whitespace after the colon, and OWS around the value") {
        f.receive("GET /r HTTP/1.1\r\nHost:example.org\r\nX-Spaces: \t value with spaces \t \r\nX-Empty:\r\n\r\n");
        REQUIRE(f.response().status == 200);
        REQUIRE(req.headers.find("Host")->second == "example.org");
        REQUIRE(req.headers.find("X-Spaces")->second == "value with spaces");
        REQUIRE(req.headers.find("X-Empty")->second.empty());
    }

    SECTION("names are case-insensitive") {
        f.receive("GET /r HTTP/1.1\r\nhOsT: example.org\r\nx-lower: a\r\n\r\n");
        REQUIRE(f.response().status == 200);
        REQUIRE(req.headers.find("Host")->second == "example.org");
        REQUIRE(req.headers.find("X-Lower")->second == "a");
    }

    SECTION("field lines with the same name are combined") {
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\nAccept: a/b\r\naccept: c/d\r\nCookie: a=1\r\nCookie: b=2\r\n\r\n");
        REQUIRE(f.response().status == 200);
        REQUIRE(req.headers.find("Accept")->second == "a/b, c/d");
        REQUIRE(req.headers.find("Cookie")->second == "a=1; b=2");
    }

    SECTION("obs-text in a value is passed on") {
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\nX-Latin1: caf\xe9\r\n\r\n");
        REQUIRE(f.response().status == 200);
        REQUIRE(req.headers.find("X-Latin1")->second == "caf\xe9");
    }

    SECTION("whitespace between the name and the colon is 400") {
        f.receive("GET /r HTTP/1.1\r\nHost : example.org\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("obs-fold is 400") {
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\nX-Folded: a\r\n b\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("whitespace before the first field line is 400") {
        f.receive("GET /r HTTP/1.1\r\n Host: example.org\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("a line without colon is 400") {
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\nnocolon\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("a name that isn't a token is 400") {
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\nX(bad): a\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("control characters in a value are 400") {
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\nX-Ctl: a\x01" "b\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("NUL in a value is 400") {
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\nX-Nul: a\0b\r\n\r\n"sv);
        REQUIRE(f.response().status == 400);
    }

    SECTION("HTTP/1.1 without Host is 400") {
        f.receive("GET /r HTTP/1.1\r\nX: a\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("two Host fields is 400") {
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("an invalid Host is 400") {
        f.receive("GET /r HTTP/1.1\r\nHost: exa mple.org\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("Host with a port, an IPv6 literal or empty is valid") {
        f.receive("GET /r HTTP/1.1\r\nHost: example.org:8080\r\n\r\nGET /r HTTP/1.1\r\nHost: [::1]:8080\r\n\r\nGET /r HTTP/1.1\r\nHost:\r\n\r\n");
        auto resps = f.responses();
        REQUIRE(resps.size() == 3);
        REQUIRE(resps[0].status == 200);
        REQUIRE(resps[1].status == 200);
        REQUIRE(resps[2].status == 200);
    }

    SECTION("HTTP/1.0 without Host is fine") {
        f.receive("GET /r HTTP/1.0\r\n\r\n");
        REQUIRE(f.response().status == 200);
    }
}

TEST_CASE("HttpHostRfcTests: message body") {
    HttpHostFixture f;
    RecordedRequest req{};
    f.addRoute(HttpMethod::post, "/r", recordingHandler(req));

    SECTION("Content-Length") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nContent-Length: 5\r\n\r\nhello");
        REQUIRE(f.response().status == 200);
        REQUIRE(req.body == "hello");
    }

    SECTION("no Content-Length or Transfer-Encoding is no body") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\n\r\nGET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto resps = f.responses();
        REQUIRE(resps.size() == 2);
        REQUIRE(resps[0].status == 200);
        REQUIRE(resps[1].status == 405);
    }

    SECTION("a list of identical Content-Length values is 400") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nContent-Length: 5, 5\r\n\r\nhello");
        REQUIRE(f.response().status == 400);
        REQUIRE(f.closed());
    }

    SECTION("different Content-Length values are 400") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nContent-Length: 5\r\nContent-Length: 6\r\n\r\nhello!");
        REQUIRE(f.response().status == 400);
        REQUIRE(f.closed());
    }
    SECTION("chunked") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nWiki\r\n5\r\npedia\r\nE\r\n in\r\n\r\nchunks.\r\n0\r\n\r\n");
        REQUIRE(f.response().status == 200);
        REQUIRE(req.body == "Wikipedia in\r\n\r\nchunks.");
    }

    SECTION("chunked with extensions and trailers, which are discarded") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nTransfer-Encoding: Chunked\r\n\r\n"
                  "4;name=value\r\nWiki\r\n5 ; quoted=\"a \\\"b\\\"\" ; flag\r\npedia\r\n0\r\nX-Checksum: abc\r\nX-Other: def\r\n\r\n");
        REQUIRE(f.response().status == 200);
        REQUIRE(req.body == "Wikipedia");
        REQUIRE(!req.headers.contains("X-Checksum"));
    }

    SECTION("chunked in pieces") {
        std::string_view const msg = "POST /r HTTP/1.1\r\nHost: example.org\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n";
        for(char const c : msg) {
            REQUIRE(f.sent().empty());
            f.receive(std::string_view{&c, 1});
        }
        REQUIRE(f.response().status == 200);
        REQUIRE(req.body == "Wikipedia");
    }

    SECTION("pipelined after a chunked request") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nTransfer-Encoding: chunked\r\n\r\n1\r\na\r\n0\r\n\r\nPOST /r HTTP/1.1\r\nHost: example.org\r\nContent-Length: 1\r\n\r\nb");
        auto resps = f.responses();
        REQUIRE(resps.size() == 2);
        REQUIRE(resps[1].status == 200);
        REQUIRE(req.body == "b");
    }
    SECTION("Transfer-Encoding and Content-Length together are 400") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nContent-Length: 3\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n0\r\n\r\n");
        REQUIRE(f.response().status == 400);
        REQUIRE(f.closed());
    }

    SECTION("Transfer-Encoding in HTTP/1.0 is 400") {
        f.receive("POST /r HTTP/1.0\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n0\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("chunked has to be the last coding") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nTransfer-Encoding: chunked, gzip\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("chunked twice is 400") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nTransfer-Encoding: chunked\r\nTransfer-Encoding: chunked\r\n\r\n");
        REQUIRE(f.response().status == 400);
    }

    SECTION("other transfer codings are 501") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nTransfer-Encoding: gzip, chunked\r\n\r\n");
        REQUIRE(f.response().status == 501);
        REQUIRE(f.closed());
    }
}

TEST_CASE("HttpHostRfcTests: Expect") {
    HttpHostFixture f;
    RecordedRequest req{};
    f.addRoute(HttpMethod::post, "/r", recordingHandler(req));

    SECTION("100-continue before the body") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nExpect: 100-continue\r\nContent-Length: 5\r\n\r\n");
        REQUIRE(f.sent() == std::vector<std::string>{"HTTP/1.1 100 Continue\r\n\r\n"});
        f.receive("hel");
        REQUIRE(f.sent().size() == 1);
        f.receive("lo");
        auto sent = f.sent();
        REQUIRE(sent.size() == 2);
        REQUIRE(parseResponse(sent[1]).status == 200);
        REQUIRE(req.body == "hello");
    }

    SECTION("no 100-continue when the body is already there") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nExpect: 100-Continue\r\nContent-Length: 5\r\n\r\nhello");
        REQUIRE(f.response().status == 200);
    }

    SECTION("100-continue for chunked") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nExpect: 100-continue\r\nTransfer-Encoding: chunked\r\n\r\n");
        REQUIRE(f.sent() == std::vector<std::string>{"HTTP/1.1 100 Continue\r\n\r\n"});
        f.receive("5\r\nhello\r\n0\r\n\r\n");
        REQUIRE(f.sent().size() == 2);
    }
    SECTION("HTTP/1.0 ignores 100-continue") {
        f.receive("POST /r HTTP/1.0\r\nExpect: 100-continue\r\nContent-Length: 5\r\n\r\n");
        REQUIRE(f.sent().empty());
        f.receive("hello");
        REQUIRE(f.response().status == 200);
    }

    SECTION("other expectations are 417") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nExpect: something-else\r\nContent-Length: 5\r\n\r\nhello");
        REQUIRE(f.response().status == 417);
        REQUIRE(f.closed());
    }
}

TEST_CASE("HttpHostRfcTests: persistence") {
    HttpHostFixture f;
    RecordedRequest req{};
    f.addRoute(HttpMethod::get, "/r", recordingHandler(req));

    SECTION("HTTP/1.1 stays open") {
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(!f.response().headers.contains("Connection"));
        REQUIRE(!f.closed());
    }

    SECTION("Connection: close closes after the response, and what was pipelined after it is dropped") {
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\nConnection: keep-alive, Close\r\n\r\nGET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto resp = f.response();
        REQUIRE(resp.status == 200);
        REQUIRE(resp.headers.find("Connection")->second == "close");
        REQUIRE(f.closed());
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.sent().size() == 1);
    }

    SECTION("HTTP/1.0 closes by default") {
        f.receive("GET /r HTTP/1.0\r\n\r\n");
        REQUIRE(f.response().headers.find("Connection")->second == "close");
        REQUIRE(f.closed());
    }

    SECTION("HTTP/1.0 with keep-alive stays open") {
        f.receive("GET /r HTTP/1.0\r\nConnection: Keep-Alive\r\n\r\n");
        REQUIRE(f.response().headers.find("Connection")->second == "keep-alive");
        REQUIRE(!f.closed());
    }
}

TEST_CASE("HttpHostRfcTests: pipelining") {
    HttpHostFixture f;
    AsyncManualResetEvent firstMayFinish;
    std::vector<std::string> handled;
    f.addRoute(HttpMethod::get, "/slow", [&](HttpRequest &req) -> Task<HttpResponse> {
        co_await firstMayFinish;
        // still valid after suspending, even though more was received
        handled.emplace_back(req.route);
        co_return HttpResponse{.status = HttpStatus::ok, .contentType = {}, .body = toBody("slow"), .headers = {}};
    });
    f.addRoute(HttpMethod::get, "/fast", [&](HttpRequest &req) -> Task<HttpResponse> {
        handled.emplace_back(req.route);
        co_return HttpResponse{.status = HttpStatus::ok, .contentType = {}, .body = toBody("fast"), .headers = {}};
    });

    SECTION("in one receive") {
        f.receive("GET /slow HTTP/1.1\r\nHost: example.org\r\n\r\nGET /fast HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.sent().empty());
        REQUIRE(handled.empty());
        firstMayFinish.set();
        auto resps = f.responses();
        REQUIRE(resps.size() == 2);
        REQUIRE(resps[0].body == "slow");
        REQUIRE(resps[1].body == "fast");
        REQUIRE(handled == std::vector<std::string>{"/slow", "/fast"});
    }

    SECTION("received while the first is being handled") {
        f.receive("GET /slow HTTP/1.1\r\nHost: example.org\r\n\r\n");
        // enough to reallocate the buffer
        for(int i = 0; i < 100; i++) {
            f.receive("GET /fast HTTP/1.1\r\nHost: example.org\r\nX-Filler: " + std::string(100, 'a') + "\r\n\r\n");
        }
        REQUIRE(f.sent().empty());
        REQUIRE(handled.empty());
        firstMayFinish.set();
        auto resps = f.responses();
        REQUIRE(resps.size() == 101);
        REQUIRE(resps[0].body == "slow");
        REQUIRE(resps[100].body == "fast");
        REQUIRE(handled.front() == "/slow");
        REQUIRE(handled.size() == 101);
    }
}

TEST_CASE("HttpHostRfcTests: routing") {
    HttpHostFixture f;
    RecordedRequest req{};
    f.addRoute(HttpMethod::get, "/r", recordingHandler(req, "content"));
    f.addRoute(HttpMethod::put, "/r", recordingHandler(req));
    f.addRoute(HttpMethod::delete_, "/other", recordingHandler(req));

    SECTION("HEAD uses the GET route without sending the content") {
        f.receive("HEAD /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto resp = f.response();
        REQUIRE(resp.status == 200);
        REQUIRE(resp.body.empty());
        REQUIRE(resp.headers.find("Content-Length")->second == "7");
        REQUIRE(resp.headers.find("Content-Type")->second == "text/plain");
        REQUIRE(req.method == HttpMethod::head);
        // the connection can still be used
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.responses()[1].body == "content");
    }

    SECTION("a HEAD route is used over the GET route") {
        HttpRequest headReq{};
        f.addRoute(HttpMethod::head, "/r", [&headReq](HttpRequest &r) -> Task<HttpResponse> {
            headReq = r;
            co_return HttpResponse{.status = HttpStatus::ok, .contentType = {}, .body = {}, .headers = {{"Content-Length", "1234"}}};
        });
        f.receive("HEAD /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto resp = f.response();
        REQUIRE(resp.headers.find("Content-Length")->second == "1234");
        REQUIRE(headReq.method == HttpMethod::head);
    }

    SECTION("a method without a route for a resource that has routes is 405 with Allow") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nContent-Length: 0\r\n\r\n");
        auto resp = f.response();
        REQUIRE(resp.status == 405);
        REQUIRE(resp.headers.find("Allow")->second == "GET, HEAD, PUT, OPTIONS");
        REQUIRE(!f.closed());
    }

    SECTION("OPTIONS lists the methods of a resource") {
        f.receive("OPTIONS /other HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto resp = f.response();
        REQUIRE(resp.status == 200);
        REQUIRE(resp.headers.find("Allow")->second == "DELETE, OPTIONS");
        REQUIRE(resp.headers.find("Content-Length")->second == "0");
    }

    SECTION("OPTIONS * lists the methods of the server") {
        f.receive("OPTIONS * HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto resp = f.response();
        REQUIRE(resp.status == 200);
        REQUIRE(resp.headers.find("Allow")->second == "DELETE, GET, HEAD, PUT, OPTIONS");
    }

    SECTION("OPTIONS for a resource without routes is 404") {
        f.receive("OPTIONS /nothing HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.response().status == 404);
    }

    SECTION("a resource without routes is 404") {
        f.receive("GET /nothing HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto resp = f.response();
        REQUIRE(resp.status == 404);
        REQUIRE(resp.body == "There is nothing at this target.\n");
    }

    SECTION("regex params after a route matched without being handled") {
        std::vector<std::string> params;
        f.addRoute(HttpMethod::get, std::make_unique<RegexRouteMatch<R"(\/user\/(\d+))">>(), [&params](HttpRequest &r) -> Task<HttpResponse> {
            params = r.regex_params;
            co_return HttpResponse{};
        });
        // matches the GET route, which isn't used for POST
        f.receive("POST /user/1 HTTP/1.1\r\nHost: example.org\r\nContent-Length: 0\r\n\r\n");
        f.receive("GET /user/2 HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.responses()[0].status == 405);
        REQUIRE(params == std::vector<std::string>{"2"});
    }
}

TEST_CASE("HttpHostRfcTests: responses") {
    HttpHostFixture f;
    HttpResponse toSend{};
    f.addRoute(HttpMethod::get, "/r", [&toSend](HttpRequest &) -> Task<HttpResponse> {
        co_return toSend;
    });
    f.addRoute(HttpMethod::post, "/r", [&toSend](HttpRequest &) -> Task<HttpResponse> {
        co_return toSend;
    });

    SECTION("an empty response has Content-Length: 0") {
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto resp = f.response();
        REQUIRE(resp.head == "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n");
    }

    SECTION("204 has no Content-Length or content") {
        toSend = HttpResponse{.status = HttpStatus::no_content, .contentType = "text/plain", .body = toBody("ignored"), .headers = {}};
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\n\r\nGET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto sent = f.sent();
        REQUIRE(sent.size() == 2);
        auto resp = parseResponse(sent[0]);
        REQUIRE(resp.head == "HTTP/1.1 204 No Content\r\n");
        REQUIRE(resp.body.empty());
    }

    SECTION("304 has no content") {
        toSend = HttpResponse{.status = HttpStatus::not_modified, .contentType = "text/plain", .body = toBody("ignored"), .headers = {{"ETag", "\"a\""}}};
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto resp = f.response();
        REQUIRE(resp.head == "HTTP/1.1 304 Not Modified\r\nETag: \"a\"\r\n");
        REQUIRE(resp.body.empty());
    }

    SECTION("the handler's framing fields are replaced") {
        toSend = HttpResponse{.status = HttpStatus::ok, .contentType = {}, .body = toBody("abc"), .headers = {{"Content-Length", "100"}, {"Transfer-Encoding", "chunked"}}};
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto resp = f.response();
        REQUIRE(resp.head == "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n");
        REQUIRE(resp.body == "abc");
    }

    SECTION("a handler's Date and Server are kept") {
        toSend = HttpResponse{.status = HttpStatus::ok, .contentType = {}, .body = {}, .headers = {{"Date", "Sun, 06 Nov 1994 08:49:37 GMT"}, {"Server", "Mine"}}};
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto resp = f.response();
        REQUIRE(resp.headers.find("Date")->second == "Sun, 06 Nov 1994 08:49:37 GMT");
        REQUIRE(resp.headers.find("Server")->second == "Mine");
    }

    SECTION("a status without a known reason phrase") {
        toSend = HttpResponse{.status = static_cast<HttpStatus>(299), .contentType = {}, .body = {}, .headers = {}};
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.sent()[0].starts_with("HTTP/1.1 299 \r\n"));
    }

    SECTION("an interim status from a handler is 500") {
        toSend = HttpResponse{.status = HttpStatus::switching_protocols, .contentType = {}, .body = {}, .headers = {}};
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.response().status == 500);
    }

    SECTION("a field value that would split the response is 500") {
        toSend = HttpResponse{.status = HttpStatus::ok, .contentType = {}, .body = {}, .headers = {{"X-Injected", "a\r\nSet-Cookie: evil"}}};
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto resp = f.response();
        REQUIRE(resp.status == 500);
        REQUIRE(!resp.headers.contains("Set-Cookie"));
    }

    SECTION("an invalid field name is 500") {
        toSend = HttpResponse{.status = HttpStatus::ok, .contentType = {}, .body = {}, .headers = {{"X Bad", "a"}}};
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.response().status == 500);
    }

    SECTION("an invalid Content-Type is 500") {
        toSend = HttpResponse{.status = HttpStatus::ok, .contentType = "text/plain\r\nX: y", .body = {}, .headers = {}};
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.response().status == 500);
    }

    SECTION("no Server by default, it tells what software to target") {
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(!f.response().headers.contains("Server"));
    }
}

TEST_CASE("HttpHostRfcTests: conditional requests") {
    HttpHostFixture f;
    f.addRoute(HttpMethod::get, "/r", [](HttpRequest &) -> Task<HttpResponse> {
        co_return HttpResponse{.status = HttpStatus::ok, .contentType = "text/plain", .body = toBody("content"), .headers = {{"ETag", "\"v1\""}, {"Last-Modified", "Sun, 06 Nov 1994 08:49:37 GMT"}}};
    });
    f.addRoute(HttpMethod::get, "/weak", [](HttpRequest &) -> Task<HttpResponse> {
        co_return HttpResponse{.status = HttpStatus::ok, .contentType = {}, .body = toBody("content"), .headers = {{"ETag", "W/\"v1\""}}};
    });
    f.addRoute(HttpMethod::post, "/r", [](HttpRequest &) -> Task<HttpResponse> {
        co_return HttpResponse{.status = HttpStatus::ok, .contentType = {}, .body = toBody("posted"), .headers = {{"ETag", "\"v1\""}}};
    });

    auto statusFor = [&f](std::string const &request) {
        HttpHostFixture *fixture = &f;
        fixture->receive(request);
        return fixture->responses().back();
    };

    SECTION("If-None-Match with the current tag is 304 without content") {
        auto resp = statusFor("GET /r HTTP/1.1\r\nHost: example.org\r\nIf-None-Match: \"v0\", \"v1\"\r\n\r\n");
        REQUIRE(resp.status == 304);
        REQUIRE(resp.body.empty());
        REQUIRE(resp.headers.find("ETag")->second == "\"v1\"");
        REQUIRE(!resp.headers.contains("Content-Type"));
    }

    SECTION("If-None-Match compares weakly") {
        REQUIRE(statusFor("GET /r HTTP/1.1\r\nHost: example.org\r\nIf-None-Match: W/\"v1\"\r\n\r\n").status == 304);
        REQUIRE(statusFor("GET /weak HTTP/1.1\r\nHost: example.org\r\nIf-None-Match: \"v1\"\r\n\r\n").status == 304);
    }

    SECTION("If-None-Match: *") {
        REQUIRE(statusFor("GET /r HTTP/1.1\r\nHost: example.org\r\nIf-None-Match: *\r\n\r\n").status == 304);
    }

    SECTION("If-None-Match with another tag") {
        auto resp = statusFor("GET /r HTTP/1.1\r\nHost: example.org\r\nIf-None-Match: \"v2\", \"v,1\"\r\n\r\n");
        REQUIRE(resp.status == 200);
        REQUIRE(resp.body == "content");
    }

    SECTION("HEAD") {
        auto resp = statusFor("HEAD /r HTTP/1.1\r\nHost: example.org\r\nIf-None-Match: \"v1\"\r\n\r\n");
        REQUIRE(resp.status == 304);
    }

    SECTION("If-Modified-Since in all three date formats") {
        REQUIRE(statusFor("GET /r HTTP/1.1\r\nHost: example.org\r\nIf-Modified-Since: Sun, 06 Nov 1994 08:49:37 GMT\r\n\r\n").status == 304);
        REQUIRE(statusFor("GET /r HTTP/1.1\r\nHost: example.org\r\nIf-Modified-Since: Sunday, 06-Nov-94 08:49:38 GMT\r\n\r\n").status == 304);
        REQUIRE(statusFor("GET /r HTTP/1.1\r\nHost: example.org\r\nIf-Modified-Since: Sun Nov  6 08:49:36 1994\r\n\r\n").status == 200);
    }

    SECTION("If-Modified-Since is ignored when If-None-Match is there") {
        REQUIRE(statusFor("GET /r HTTP/1.1\r\nHost: example.org\r\nIf-None-Match: \"v2\"\r\nIf-Modified-Since: Sun, 06 Nov 1994 08:49:37 GMT\r\n\r\n").status == 200);
    }

    SECTION("an invalid If-Modified-Since is ignored") {
        REQUIRE(statusFor("GET /r HTTP/1.1\r\nHost: example.org\r\nIf-Modified-Since: yesterday\r\n\r\n").status == 200);
    }

    SECTION("If-Match compares strongly") {
        REQUIRE(statusFor("GET /r HTTP/1.1\r\nHost: example.org\r\nIf-Match: \"v1\"\r\n\r\n").status == 200);
        REQUIRE(statusFor("GET /r HTTP/1.1\r\nHost: example.org\r\nIf-Match: *\r\n\r\n").status == 200);
        REQUIRE(statusFor("GET /r HTTP/1.1\r\nHost: example.org\r\nIf-Match: \"v2\"\r\n\r\n").status == 412);
        REQUIRE(statusFor("GET /r HTTP/1.1\r\nHost: example.org\r\nIf-Match: W/\"v1\"\r\n\r\n").status == 412);
        auto resp = statusFor("GET /weak HTTP/1.1\r\nHost: example.org\r\nIf-Match: \"v1\"\r\n\r\n");
        REQUIRE(resp.status == 412);
        REQUIRE(resp.body.empty());
    }

    SECTION("If-Unmodified-Since") {
        REQUIRE(statusFor("GET /r HTTP/1.1\r\nHost: example.org\r\nIf-Unmodified-Since: Sun, 06 Nov 1994 08:49:37 GMT\r\n\r\n").status == 200);
        REQUIRE(statusFor("GET /r HTTP/1.1\r\nHost: example.org\r\nIf-Unmodified-Since: Sun, 06 Nov 1994 08:49:36 GMT\r\n\r\n").status == 412);
    }

    SECTION("preconditions of other methods are the route's to evaluate") {
        REQUIRE(statusFor("POST /r HTTP/1.1\r\nHost: example.org\r\nIf-None-Match: \"v1\"\r\nContent-Length: 0\r\n\r\n").status == 200);
    }
}

TEST_CASE("HttpHostRfcTests: invalid Content-Length") {
    RecordedRequest req{};
    for(auto cl : {"abc"sv, "-1"sv, "+5"sv, "5,"sv, "99999999999999999999999"sv, "0x5"sv}) {
        HttpHostFixture f;
        f.addRoute(HttpMethod::post, "/r", recordingHandler(req));
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nContent-Length: " + std::string{cl} + "\r\n\r\nhello");
        INFO(cl);
        REQUIRE(f.response().status == 400);
    }
}

TEST_CASE("HttpHostRfcTests: invalid chunked bodies") {
    RecordedRequest req{};
    for(auto body : {"zz\r\nabc\r\n0\r\n\r\n"sv, "3\r\nabcd\r\n0\r\n\r\n"sv, "3\r\nabc0\r\n\r\n"sv, "11111111111111111\r\n"sv, "3 \r\nabc\r\n0\r\n\r\n"sv,
                      "3;\r\nabc\r\n0\r\n\r\n"sv, "3\nabc\r\n0\r\n\r\n"sv, "3\r\nabc\r\n0\r\nbad trailer\r\n\r\n"sv, "-3\r\nabc\r\n0\r\n\r\n"sv}) {
        HttpHostFixture f;
        f.addRoute(HttpMethod::post, "/r", recordingHandler(req));
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nTransfer-Encoding: chunked\r\n\r\n" + std::string{body});
        INFO(body);
        REQUIRE(f.response().status == 400);
        REQUIRE(f.closed());
    }
}

TEST_CASE("HttpHostRfcTests: a rejected request doesn't get a 100 Continue") {
    HttpHostFixture f{Properties{{"MaxBodySize", Ichor::v1::make_any<uint64_t>(3ULL)}}};
    f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nExpect: 100-continue\r\nContent-Length: 5\r\n\r\n");
    REQUIRE(f.response().status == 413);
}

TEST_CASE("HttpHostRfcTests: a handler can close the connection") {
    HttpHostFixture f;
    f.addRoute(HttpMethod::get, "/r", [](HttpRequest &) -> Task<HttpResponse> {
        co_return HttpResponse{.status = HttpStatus::ok, .contentType = {}, .body = {}, .headers = {{"Connection", "close"}}};
    });
    f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
    REQUIRE(f.response().headers.find("Connection")->second == "close");
    REQUIRE(f.closed());
}

TEST_CASE("HttpHostRfcTests: SendServerHeader") {
    HttpHostFixture f{Properties{{"SendServerHeader", Ichor::v1::make_any<bool>(true)}}};
    f.addRoute(HttpMethod::get, "/r", [](HttpRequest &) -> Task<HttpResponse> {
        co_return HttpResponse{};
    });
    f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
    REQUIRE(f.response().headers.find("Server")->second == "Ichor");
}

TEST_CASE("HttpHostRfcTests: a request received before the receive handler is set") {
    HttpHostFixture f{{}, "GET /r HTTP/1.1\r\nHost: example.org\r\n\r\n"};
    RecordedRequest req{};
    f.addRoute(HttpMethod::get, "/r", recordingHandler(req));
    f.runEvents();
    REQUIRE(f.response().status == 200);
}

TEST_CASE("HttpHostRfcTests: unbounded input") {
    HttpHostFixture f{Properties{{"MaxRequestLineSize", Ichor::v1::make_any<uint64_t>(100ULL)}, {"MaxHeaderSize", Ichor::v1::make_any<uint64_t>(200ULL)}, {"MaxBodySize", Ichor::v1::make_any<uint64_t>(300ULL)}}};
    AsyncManualResetEvent mayFinish;
    f.addRoute(HttpMethod::get, "/slow", [&mayFinish](HttpRequest &) -> Task<HttpResponse> {
        co_await mayFinish;
        co_return HttpResponse{};
    });
    RecordedRequest req{};
    f.addRoute(HttpMethod::post, "/r", recordingHandler(req));

    SECTION("empty lines before the request line") {
        for(int i = 0; i < 60; i++) {
            f.receive("\r\n");
        }
        REQUIRE(f.response().status == 400);
        REQUIRE(f.closed());
    }

    SECTION("chunk extensions") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nTransfer-Encoding: chunked\r\n\r\n");
        for(int i = 0; i < 10; i++) {
            f.receive("1;ext=" + std::string(30, 'a') + "\r\nb\r\n");
        }
        REQUIRE(f.response().status == 400);
        REQUIRE(f.closed());
    }

    SECTION("pipelined while a request is being handled") {
        f.receive("GET /slow HTTP/1.1\r\nHost: example.org\r\n\r\n");
        f.receive("GET /slow HTTP/1.1\r\nHost: example.org\r\n\r\n" + std::string(450, '\r'));
        REQUIRE(!f.closed());
        f.receive(std::string(100, '\r'));
        REQUIRE(f.closed());
        mayFinish.set();
        // The request that was being handled still gets its response
        REQUIRE(f.response().status == 200);
    }
}

TEST_CASE("HttpHostRfcTests: a 2xx to CONNECT has no framing and closes") {
    HttpHostFixture f;
    f.addRoute(HttpMethod::connect, "example.org:443", [](HttpRequest &) -> Task<HttpResponse> {
        co_return HttpResponse{.status = HttpStatus::ok, .contentType = {}, .body = toBody("ignored"), .headers = {}};
    });
    f.receive("CONNECT example.org:443 HTTP/1.1\r\nHost: example.org:443\r\n\r\n");
    auto resp = f.response();
    REQUIRE(resp.status == 200);
    REQUIRE(!resp.headers.contains("Content-Length"));
    REQUIRE(resp.body.empty());
    REQUIRE(f.closed());
}

namespace {
    /// Decodes a chunked body, nothing if it isn't valid or complete
    tl::optional<std::string> decodeChunked(std::string_view body) {
        std::string ret;
        while(true) {
            auto const lineEnd = body.find("\r\n");
            if(lineEnd == std::string_view::npos) {
                return tl::nullopt;
            }
            auto const size = std::stoul(std::string{body.substr(0, lineEnd)}, nullptr, 16);
            body.remove_prefix(lineEnd + 2);
            if(size == 0) {
                return body == "\r\n" ? tl::optional<std::string>{ret} : tl::nullopt;
            }
            if(body.size() < size + 2 || body.substr(size, 2) != "\r\n") {
                return tl::nullopt;
            }
            ret += body.substr(0, size);
            body.remove_prefix(size + 2);
        }
    }

    Properties timeouts(uint64_t idleMs, uint64_t requestMs, uint64_t lingerMs) {
        return Properties{{"IdleTimeoutMs", Ichor::v1::make_any<uint64_t>(idleMs)}, {"RequestTimeoutMs", Ichor::v1::make_any<uint64_t>(requestMs)}, {"LingerTimeoutMs", Ichor::v1::make_any<uint64_t>(lingerMs)}};
    }
}

TEST_CASE("HttpHostRfcTests: graceful close") {
    HttpHostFixture f{timeouts(60'000, 60'000, 5), {}, true};
    RecordedRequest req{};
    f.addRoute(HttpMethod::get, "/r", recordingHandler(req));

    SECTION("the sending side first, then the connection after the linger timeout") {
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\nConnection: close\r\n\r\n");
        REQUIRE(f.response().headers.find("Connection")->second == "close");
        REQUIRE(f.conn->getService().sendShutdown);
        REQUIRE(!f.stopped());
        // what the client still sends is dropped
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.sent().size() == 1);
        f.tick();
        REQUIRE(!f.stopped());
        std::this_thread::sleep_for(20ms);
        f.tick();
        REQUIRE(f.stopped());
    }

    SECTION("after an invalid request") {
        f.receive("GET /r HTTP/1.1\r\n\r\n");
        REQUIRE(f.response().status == 400);
        REQUIRE(f.conn->getService().sendShutdown);
        REQUIRE(!f.stopped());
    }
}

TEST_CASE("HttpHostRfcTests: a linger timeout of 0 closes right away") {
    HttpHostFixture f{timeouts(60'000, 60'000, 0), {}, true};
    RecordedRequest req{};
    f.addRoute(HttpMethod::get, "/r", recordingHandler(req));
    f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\nConnection: close\r\n\r\n");
    REQUIRE(!f.conn->getService().sendShutdown);
    REQUIRE(f.stopped());
}

TEST_CASE("HttpHostRfcTests: without a timer factory connections are closed right away") {
    HttpHostFixture f;
    RecordedRequest req{};
    f.addRoute(HttpMethod::get, "/r", recordingHandler(req));
    f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\nConnection: close\r\n\r\n");
    REQUIRE(!f.conn->getService().sendShutdown);
    REQUIRE(f.stopped());
}

TEST_CASE("HttpHostRfcTests: timeouts") {
    RecordedRequest req{};

    SECTION("the timer checks often enough for the shortest timeout") {
        HttpHostFixture f{timeouts(1'000, 400, 2'000), {}, true};
        REQUIRE(f.timerFactory->getService().timers.size() == 1);
        REQUIRE(f.timerFactory->getService().timers[0]->interval == 100ms);
        REQUIRE(f.timerFactory->getService().timers[0]->running);
    }

    SECTION("an idle connection is closed") {
        HttpHostFixture f{timeouts(5, 60'000, 60'000), {}, true};
        f.tick();
        REQUIRE(!f.closed());
        std::this_thread::sleep_for(20ms);
        f.tick();
        REQUIRE(f.closed());
        REQUIRE(f.sent().empty());
    }

    SECTION("the idle timeout starts again after a response") {
        HttpHostFixture f{timeouts(50, 60'000, 60'000), {}, true};
        f.addRoute(HttpMethod::get, "/r", recordingHandler(req));
        std::this_thread::sleep_for(30ms);
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        std::this_thread::sleep_for(30ms);
        f.tick();
        REQUIRE(!f.closed());
        std::this_thread::sleep_for(40ms);
        f.tick();
        REQUIRE(f.closed());
    }

    SECTION("a request that takes too long gets 408") {
        HttpHostFixture f{timeouts(60'000, 5, 60'000), {}, true};
        f.addRoute(HttpMethod::get, "/r", recordingHandler(req));
        f.receive("GET /r HTTP/1.1\r\nHost: exam");
        f.tick();
        REQUIRE(f.sent().empty());
        std::this_thread::sleep_for(20ms);
        f.tick();
        auto resp = f.response();
        REQUIRE(resp.status == 408);
        REQUIRE(resp.headers.find("Connection")->second == "close");
        REQUIRE(f.closed());
        // the rest of it doesn't make it a request anymore
        f.receive("ple.org\r\n\r\n");
        REQUIRE(f.sent().size() == 1);
    }

    SECTION("the request timeout runs from the start of the request, receiving more doesn't extend it") {
        HttpHostFixture f{timeouts(60'000, 30, 60'000), {}, true};
        f.addRoute(HttpMethod::get, "/r", recordingHandler(req));
        f.receive("GET /r HTTP/1.1\r\n");
        for(int i = 0; i < 4; i++) {
            std::this_thread::sleep_for(10ms);
            f.receive("X: y\r\n");
        }
        f.tick();
        REQUIRE(f.response().status == 408);
    }

    SECTION("a handler that takes long is not a request timeout") {
        HttpHostFixture f{timeouts(60'000, 5, 60'000), {}, true};
        AsyncManualResetEvent mayFinish;
        f.addRoute(HttpMethod::get, "/slow", [&mayFinish](HttpRequest &) -> Task<HttpResponse> {
            co_await mayFinish;
            co_return HttpResponse{};
        });
        f.receive("GET /slow HTTP/1.1\r\nHost: example.org\r\n\r\n");
        std::this_thread::sleep_for(20ms);
        f.tick();
        REQUIRE(f.sent().empty());
        mayFinish.set();
        REQUIRE(f.response().status == 200);
        REQUIRE(!f.closed());
    }

    SECTION("0 disables a timeout") {
        HttpHostFixture f{timeouts(0, 0, 5), {}, true};
        f.receive("GET /r HTTP/1.1\r\nHo");
        std::this_thread::sleep_for(20ms);
        f.tick();
        REQUIRE(f.sent().empty());
        REQUIRE(!f.closed());
    }
}

TEST_CASE("HttpHostRfcTests: streaming responses") {
    HttpHostFixture f;

    SECTION("chunked for HTTP/1.1") {
        f.addStreamingRoute(HttpMethod::get, "/stream", [](HttpRequest &, IHttpResponseWriter &writer) -> Task<void> {
            {
                HttpResponse response{.status = HttpStatus::ok, .contentType = "text/event-stream", .body = toBody("first"), .headers = {}};
                auto const result = co_await writer.start(std::move(response));
                REQUIRE(result);
            }
            {
                auto const result = co_await writer.write(toBody("Hello"));
                REQUIRE(result);
            }
            {
                auto const result = co_await writer.write({});
                REQUIRE(result);
            }
            {
                auto const result = co_await writer.write(toBody(" world, this is a longer part"));
                REQUIRE(result);
            }
            // only once
            {
                HttpResponse response{};
                auto const result = co_await writer.start(std::move(response));
                REQUIRE(!result);
            }
            REQUIRE(!writer.upgrade());
        });
        f.receive("GET /stream HTTP/1.1\r\nHost: example.org\r\n\r\nGET /stream HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto all = f.allSent();
        auto const second = all.find("HTTP/1.1", 1);
        REQUIRE(second != std::string::npos);
        auto resp = parseResponse(all.substr(0, second));
        REQUIRE(resp.head == "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nTransfer-Encoding: chunked\r\n");
        REQUIRE(decodeChunked(resp.body) == "firstHello world, this is a longer part");
        REQUIRE(resp.body.starts_with("5\r\nfirst\r\n5\r\nHello\r\n1d\r\n"));
        // the connection stays open, the next request gets the same
        REQUIRE(decodeChunked(parseResponse(all.substr(second)).body) == "firstHello world, this is a longer part");
        REQUIRE(!f.closed());
    }

    SECTION("a 200 when the handler writes without starting, or doesn't write at all") {
        f.addStreamingRoute(HttpMethod::get, "/write", [](HttpRequest &, IHttpResponseWriter &writer) -> Task<void> {
            {
                auto const result = co_await writer.write(toBody("abc"));
                REQUIRE(result);
            }
        });
        f.addStreamingRoute(HttpMethod::get, "/nothing", [](HttpRequest &, IHttpResponseWriter &) -> Task<void> {
            co_return;
        });
        f.receive("GET /write HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto resp = parseResponse(f.allSent());
        REQUIRE(resp.status == 200);
        REQUIRE(decodeChunked(resp.body) == "abc");
        f.conn->getService().sentMessages.clear();
        f.receive("GET /nothing HTTP/1.1\r\nHost: example.org\r\n\r\n");
        resp = parseResponse(f.allSent());
        REQUIRE(resp.status == 200);
        REQUIRE(resp.body == "0\r\n\r\n");
    }

    SECTION("close-delimited for HTTP/1.0") {
        f.addStreamingRoute(HttpMethod::get, "/stream", [](HttpRequest &, IHttpResponseWriter &writer) -> Task<void> {
            {
                auto const result = co_await writer.write(toBody("Hello"));
                REQUIRE(result);
            }
            {
                auto const result = co_await writer.write(toBody(" world"));
                REQUIRE(result);
            }
        });
        f.receive("GET /stream HTTP/1.0\r\nConnection: keep-alive\r\n\r\n");
        auto resp = parseResponse(f.allSent());
        REQUIRE(resp.head == "HTTP/1.1 200 OK\r\nConnection: close\r\n");
        REQUIRE(resp.body == "Hello world");
        REQUIRE(f.closed());
    }

    SECTION("HEAD and 204 have no content") {
        f.addStreamingRoute(HttpMethod::get, "/stream", [](HttpRequest &, IHttpResponseWriter &writer) -> Task<void> {
            {
                auto const result = co_await writer.write(toBody("Hello"));
                REQUIRE(result);
            }
        });
        f.addStreamingRoute(HttpMethod::get, "/none", [](HttpRequest &, IHttpResponseWriter &writer) -> Task<void> {
            {
                HttpResponse response{.status = HttpStatus::no_content, .contentType = {}, .body = toBody("ignored"), .headers = {}};
                auto const result = co_await writer.start(std::move(response));
                REQUIRE(result);
            }
            {
                auto const result = co_await writer.write(toBody("ignored"));
                REQUIRE(result);
            }
        });
        f.receive("HEAD /stream HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto resp = parseResponse(f.allSent());
        REQUIRE(resp.head == "HTTP/1.1 200 OK\r\n");
        REQUIRE(resp.body.empty());
        f.conn->getService().sentMessages.clear();
        f.receive("GET /none HTTP/1.1\r\nHost: example.org\r\n\r\n");
        resp = parseResponse(f.allSent());
        REQUIRE(resp.head == "HTTP/1.1 204 No Content\r\n");
        REQUIRE(resp.body.empty());
        REQUIRE(!f.closed());
    }

    SECTION("the default for hosts that can't stream sends it all at once") {
        auto response = [](HttpRequest &req) -> AsyncGenerator<HttpResponse> {
            co_return co_await Ichor::v1::Detail::runBuffered([](HttpRequest &, IHttpResponseWriter &writer) -> Task<void> {
                {
                    HttpResponse created{.status = HttpStatus::created, .contentType = "text/plain", .body = toBody("a"), .headers = {}};
                    auto const result = co_await writer.start(std::move(created));
                    REQUIRE(result);
                }
                {
                    auto const result = co_await writer.write(toBody("bc"));
                    REQUIRE(result);
                }
                REQUIRE(!writer.upgrade());
            }, req);
        };
        HttpRequest req{};
        auto gen = response(req);
        auto it = gen.begin();
        REQUIRE(it.get_finished());
        auto resp = it.get_value<HttpResponse>();
        REQUIRE(resp.status == HttpStatus::created);
        REQUIRE(resp.body == toBody("abc"));
    }
}

TEST_CASE("HttpHostRfcTests: upgrade") {
    HttpHostFixture f;
    std::shared_ptr<IHttpUpgradedConnection> upgraded;
    std::string received;
    auto upgradingHandler = [&](HttpResponse const &response) {
        return [&upgraded, &received, response](HttpRequest &, IHttpResponseWriter &writer) -> Task<void> {
            co_await writer.start(response);
            auto connection = writer.upgrade();
            if(connection) {
                upgraded = *connection;
                upgraded->setReceiveHandler([&received](std::span<uint8_t const> data) {
                    received.append(reinterpret_cast<char const*>(data.data()), data.size());
                });
            }
        };
    };
    HttpResponse const switchToWebsocket{.status = HttpStatus::switching_protocols, .contentType = {}, .body = {}, .headers = {{"Upgrade", "websocket"}}};
    f.addStreamingRoute(HttpMethod::get, "/chat", upgradingHandler(switchToWebsocket));

    SECTION("101 hands the connection over") {
        f.receive("GET /chat HTTP/1.1\r\nHost: example.org\r\nUpgrade: h2c, WebSocket\r\nConnection: keep-alive, Upgrade\r\n\r\nalready sent");
        auto resp = f.response();
        REQUIRE(resp.head == "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n");
        REQUIRE(upgraded);
        REQUIRE(upgraded->isOpen());
        REQUIRE(received == "already sent");
        // what follows is no longer HTTP
        f.receive("GET /chat HTTP/1.1\r\n\r\n");
        REQUIRE(received == "already sentGET /chat HTTP/1.1\r\n\r\n");
        REQUIRE(f.sent().size() == 1);
        // A named lambda: the coroutine refers to its captures, which a temporary would destroy before it runs
        auto sendRaw = [&]() -> AsyncGenerator<IchorBehaviour> {
            {
                auto const result = co_await upgraded->sendAsync(toBody("raw"));
                REQUIRE(result);
            }
            co_return {};
        };
        auto sendGen = sendRaw();
        REQUIRE(sendGen.begin().get_finished());
        REQUIRE(f.sent().back() == "raw");
        REQUIRE(!f.closed());
        upgraded->close();
        REQUIRE(!upgraded->isOpen());
        REQUIRE(f.closed());
    }

    SECTION("101 without the client asking for it is 500") {
        f.receive("GET /chat HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(parseResponse(f.allSent()).status == 500);
        REQUIRE(!upgraded);
    }

    SECTION("101 to a protocol the client didn't ask for is 500") {
        f.receive("GET /chat HTTP/1.1\r\nHost: example.org\r\nUpgrade: h2c\r\nConnection: Upgrade\r\n\r\n");
        REQUIRE(parseResponse(f.allSent()).status == 500);
    }

    SECTION("Upgrade has to be a connection option") {
        f.receive("GET /chat HTTP/1.1\r\nHost: example.org\r\nUpgrade: websocket\r\n\r\n");
        REQUIRE(parseResponse(f.allSent()).status == 500);
    }

    SECTION("Upgrade is ignored in HTTP/1.0") {
        f.receive("GET /chat HTTP/1.0\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n\r\n");
        REQUIRE(parseResponse(f.allSent()).status == 500);
    }

    SECTION("101 without taking the connection over closes it") {
        f.addStreamingRoute(HttpMethod::get, "/noupgrade", [&switchToWebsocket](HttpRequest &, IHttpResponseWriter &writer) -> Task<void> {
            co_await writer.start(switchToWebsocket);
        });
        f.receive("GET /noupgrade HTTP/1.1\r\nHost: example.org\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n\r\n");
        REQUIRE(f.response().status == 101);
        REQUIRE(f.closed());
    }

    SECTION("a response that doesn't switch protocols can't be upgraded") {
        f.addStreamingRoute(HttpMethod::get, "/ok", upgradingHandler(HttpResponse{}));
        f.receive("GET /ok HTTP/1.1\r\nHost: example.org\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n\r\n");
        REQUIRE(!upgraded);
        REQUIRE(parseResponse(f.allSent()).status == 200);
        REQUIRE(!f.closed());
    }

    SECTION("a 2xx to CONNECT is a tunnel") {
        f.addStreamingRoute(HttpMethod::connect, "example.org:443", upgradingHandler(HttpResponse{}));
        f.receive("CONNECT example.org:443 HTTP/1.1\r\nHost: example.org:443\r\n\r\n\x16\x03\x01");
        auto resp = f.response();
        REQUIRE(resp.head == "HTTP/1.1 200 OK\r\n");
        REQUIRE(upgraded);
        REQUIRE(received == "\x16\x03\x01");
        REQUIRE(!f.closed());
    }

    SECTION("the upgraded connection closes with the host") {
        f.receive("GET /chat HTTP/1.1\r\nHost: example.org\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n\r\n");
        REQUIRE(upgraded);
        REQUIRE(upgraded->isOpen());
        // the states the DependencyManager would take it through
        REQUIRE(f.svc->setInjected());
        REQUIRE(f.svc->setUninjected());
        auto stopGen = f.svc->stop();
        REQUIRE(stopGen.begin().get_finished());
        REQUIRE(!upgraded->isOpen());
    }
}

TEST_CASE("HttpHostRfcTests: 205 has no content") {
    HttpHostFixture f;
    f.addRoute(HttpMethod::post, "/form", [](HttpRequest &) -> Task<HttpResponse> {
        co_return HttpResponse{.status = HttpStatus::reset_content, .contentType = "text/plain", .body = toBody("ignored"), .headers = {}};
    });
    f.addStreamingRoute(HttpMethod::post, "/stream", [](HttpRequest &, IHttpResponseWriter &writer) -> Task<void> {
        HttpResponse response{.status = HttpStatus::reset_content, .contentType = {}, .body = toBody("ignored"), .headers = {}};
        {
            auto const result = co_await writer.start(std::move(response));
            REQUIRE(result);
        }
        {
            auto const result = co_await writer.write(toBody("ignored too"));
            REQUIRE(result);
        }
    });
    f.receive("POST /form HTTP/1.1\r\nHost: example.org\r\nContent-Length: 0\r\n\r\nPOST /stream HTTP/1.1\r\nHost: example.org\r\nContent-Length: 0\r\n\r\n");
    auto all = f.allSent();
    auto const second = all.find("HTTP/1.1", 1);
    REQUIRE(second != std::string::npos);
    for(auto const &msg : {all.substr(0, second), all.substr(second)}) {
        auto resp = parseResponse(msg);
        REQUIRE(resp.status == 205);
        REQUIRE(resp.headers.find("Content-Length")->second == "0");
        REQUIRE(resp.body.empty());
    }
    REQUIRE(!f.closed());
}

TEST_CASE("HttpHostRfcTests: TRACE") {
    SECTION("off by default") {
        HttpHostFixture f;
        RecordedRequest req{};
        f.addRoute(HttpMethod::get, "/r", recordingHandler(req));
        f.receive("TRACE /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto resp = f.response();
        REQUIRE(resp.status == 405);
        REQUIRE(resp.headers.find("Allow")->second == "GET, HEAD, OPTIONS");
    }

    SECTION("reflects the request without sensitive fields") {
        HttpHostFixture f{Properties{{"EnableTrace", Ichor::v1::make_any<bool>(true)}}};
        f.receive("TRACE /anything?x=1 HTTP/1.1\r\nHost: example.org\r\nAuthorization: Basic c2VjcmV0\r\nX-Custom:  value\r\ncookie: session=1\r\nProxy-Authorization: Basic c2VjcmV0\r\n\r\n");
        auto resp = f.response();
        REQUIRE(resp.status == 200);
        REQUIRE(resp.headers.find("Content-Type")->second == "message/http");
        REQUIRE(resp.body == "TRACE /anything?x=1 HTTP/1.1\r\nHost: example.org\r\nX-Custom:  value\r\n\r\n");
        REQUIRE(!f.closed());
    }

    SECTION("a TRACE route is used instead") {
        HttpHostFixture f{Properties{{"EnableTrace", Ichor::v1::make_any<bool>(true)}}};
        RecordedRequest req{};
        f.addRoute(HttpMethod::trace, "/r", recordingHandler(req, "mine"));
        f.receive("TRACE /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.response().body == "mine");
    }
}

TEST_CASE("HttpHostRfcTests: required fields of a handler's response") {
    HttpHostFixture f;
    HttpResponse toSend{};
    f.addRoute(HttpMethod::get, "/r", [](HttpRequest &) -> Task<HttpResponse> {
        co_return HttpResponse{};
    });
    f.addRoute(HttpMethod::post, "/r", [&toSend](HttpRequest &) -> Task<HttpResponse> {
        co_return toSend;
    });
    auto logged = [&f](std::string_view text) {
        return std::ranges::any_of(f.logger.getService().logs, [text](std::string const &log) { return log.find(text) != std::string::npos; });
    };

    SECTION("a 405 gets Allow") {
        toSend.status = HttpStatus::method_not_allowed;
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nContent-Length: 0\r\n\r\n");
        auto resp = f.response();
        REQUIRE(resp.status == 405);
        REQUIRE(resp.headers.find("Allow")->second == "GET, HEAD, OPTIONS");
    }

    SECTION("a 405 that has Allow keeps it") {
        toSend = HttpResponse{.status = HttpStatus::method_not_allowed, .contentType = {}, .body = {}, .headers = {{"Allow", "GET"}}};
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nContent-Length: 0\r\n\r\n");
        REQUIRE(f.response().headers.find("Allow")->second == "GET");
    }

    SECTION("a 401 without WWW-Authenticate is a handler error") {
        toSend.status = HttpStatus::unauthorized;
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nContent-Length: 0\r\n\r\n");
        REQUIRE(f.response().status == 401);
        REQUIRE(logged("WWW-Authenticate"));
    }

    SECTION("a 401 with WWW-Authenticate") {
        toSend = HttpResponse{.status = HttpStatus::unauthorized, .contentType = {}, .body = {}, .headers = {{"WWW-Authenticate", "Basic realm=\"x\""}}};
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nContent-Length: 0\r\n\r\n");
        REQUIRE(f.response().status == 401);
        REQUIRE(!logged("WWW-Authenticate"));
    }
}

TEST_CASE("HttpHostRfcTests: a method longer than any Ichor implements is 501") {
    HttpHostFixture f{Properties{{"MaxRequestLineSize", Ichor::v1::make_any<uint64_t>(30ULL)}}};
    f.receive("ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJ");
    REQUIRE(f.response().status == 501);
}

TEST_CASE("HttpHostRfcTests: stopping while the timer factory stops") {
    HttpHostFixture f{{}, {}, true};
    REQUIRE(f.timerFactory->getService().timers.size() == 1);
    // The factory stops before or at the same time as the host, the host still has a TimerRef to one of its timers
    f.timerFactory->getService().stopping = true;
    REQUIRE(f.svc->setInjected());
    REQUIRE(f.svc->setUninjected());
    auto stopGen = f.svc->stop();
    REQUIRE(stopGen.begin().get_finished());
    REQUIRE(f.timerFactory->getService().released == 0);
}

TEST_CASE("HttpHostRfcTests: stopping releases the timeout timer") {
    HttpHostFixture f{{}, {}, true};
    auto &factory = f.timerFactory->getService();
    REQUIRE(factory.timers.size() == 1);
    REQUIRE(factory.timers[0]->running);
    REQUIRE(f.svc->setInjected());
    REQUIRE(f.svc->setUninjected());
    auto stopGen = f.svc->stop();
    REQUIRE(stopGen.begin().get_finished());
    // the host doesn't leave a running timer behind in a factory that keeps existing
    REQUIRE(factory.released == 1);
    REQUIRE(!factory.timers[0]->running);
}

TEST_CASE("HttpHostRfcTests: chunk framing") {
    RecordedRequest req{};

    SECTION("small chunks with more framing than MaxHeaderSize") {
        HttpHostFixture f;
        f.addRoute(HttpMethod::post, "/r", recordingHandler(req));
        std::string body;
        for(int i = 0; i < 20'000; i++) {
            body += "10\r\n0123456789abcdef\r\n";
        }
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nTransfer-Encoding: chunked\r\n\r\n" + body + "0\r\n\r\n");
        REQUIRE(f.response().status == 200);
        REQUIRE(req.body.size() == 320'000);
    }

    // One byte chunks have 5 bytes of framing, the last chunk 3: up to MaxHeaderSize + the content
    for(auto const &[chunks, status] : {std::pair{49, 200U}, std::pair{51, 400U}}) {
        HttpHostFixture f{Properties{{"MaxHeaderSize", Ichor::v1::make_any<uint64_t>(200ULL)}}};
        f.addRoute(HttpMethod::post, "/r", recordingHandler(req));
        std::string body;
        for(int i = 0; i < chunks; i++) {
            body += "1\r\na\r\n";
        }
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nTransfer-Encoding: chunked\r\n\r\n" + body + "0\r\n\r\n");
        INFO(chunks);
        REQUIRE(f.response().status == status);
    }
}

TEST_CASE("HttpHostRfcTests: a request received in parts is parsed once") {
    Detail::HttpRequestLimits const limits{};
    Detail::HttpRequestParseState state;
    std::string buffer = "\r\nGET /r HTTP/1.1\r\nHo";
    REQUIRE(Detail::parseHttpRequest(buffer, limits, state).status == Detail::HttpRequestParseStatus::INCOMPLETE);
    REQUIRE(state.requestLineChecked);
    REQUIRE(state.start + state.scanned == buffer.size());

    // Changing what was parsed already shows it isn't parsed again
    SECTION("the request line") {
        buffer.replace(buffer.find("/r"), 2, "/x");
        buffer += "st: example.org\r\n\r\n";
        auto result = Detail::parseHttpRequest(buffer, limits, state);
        REQUIRE(result.status == Detail::HttpRequestParseStatus::COMPLETE);
        REQUIRE(result.request.target == "/r");
        REQUIRE(result.consumed == buffer.size());
    }

    SECTION("the header section") {
        buffer += "st: example.org\r\nContent-Length: 6\r\n\r\nab";
        REQUIRE(Detail::parseHttpRequest(buffer, limits, state).status == Detail::HttpRequestParseStatus::INCOMPLETE);
        REQUIRE(state.headerDone);
        buffer.replace(buffer.find("example.org"), 11, "exa mple.or");
        buffer += "cdef";
        auto result = Detail::parseHttpRequest(buffer, limits, state);
        REQUIRE(result.status == Detail::HttpRequestParseStatus::COMPLETE);
        REQUIRE(result.request.headers.find("Host")->second == "example.org");
        REQUIRE(std::string{result.request.body.begin(), result.request.body.end()} == "abcdef");
    }

    SECTION("chunks") {
        buffer += "st: example.org\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n2\r\nd";
        REQUIRE(Detail::parseHttpRequest(buffer, limits, state).status == Detail::HttpRequestParseStatus::INCOMPLETE);
        buffer.replace(buffer.find("abc"), 3, "xyz");
        buffer += "e\r\n0\r\nX-Trailer: a";
        REQUIRE(Detail::parseHttpRequest(buffer, limits, state).status == Detail::HttpRequestParseStatus::INCOMPLETE);
        buffer += "\r\n\r\n";
        auto result = Detail::parseHttpRequest(buffer, limits, state);
        REQUIRE(result.status == Detail::HttpRequestParseStatus::COMPLETE);
        REQUIRE(std::string{result.request.body.begin(), result.request.body.end()} == "abcde");
        REQUIRE(result.consumed == buffer.size());
    }
}

TEST_CASE("HttpHostRfcTests: pipelined requests split over receives") {
    HttpHostFixture f;
    std::vector<std::string> bodies;
    f.addRoute(HttpMethod::post, "/r", [&bodies](HttpRequest &req) -> Task<HttpResponse> {
        bodies.emplace_back(req.body.begin(), req.body.end() - 1);
        co_return HttpResponse{};
    });
    std::string all;
    for(int i = 0; i < 1'000; i++) {
        auto const body = std::to_string(i);
        all += "POST /r HTTP/1.1\r\nHost: example.org\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    }
    for(size_t i = 0; i < all.size(); i += 7) {
        f.receive(std::string_view{all}.substr(i, 7));
    }
    REQUIRE(bodies.size() == 1'000);
    for(size_t i = 0; i < bodies.size(); i++) {
        REQUIRE(bodies[i] == std::to_string(i));
    }
    REQUIRE(f.sent().size() == 1'000);
    REQUIRE(!f.closed());
}

TEST_CASE("HttpHostRfcTests: closing an upgraded connection in the handler lingers") {
    HttpHostFixture f{timeouts(0, 0, 5), {}, true};
    f.addStreamingRoute(HttpMethod::connect, "example.org:443", [](HttpRequest &, IHttpResponseWriter &writer) -> Task<void> {
        HttpResponse ok{};
        co_await writer.start(std::move(ok));
        auto connection = writer.upgrade();
        REQUIRE(connection);
        // e.g. the upstream can't be reached
        (*connection)->close();
    });
    f.receive("CONNECT example.org:443 HTTP/1.1\r\nHost: example.org:443\r\n\r\n");
    REQUIRE(f.response().status == 200);
    REQUIRE(f.conn->getService().sendShutdown);
    REQUIRE(!f.stopped());
    std::this_thread::sleep_for(20ms);
    f.tick();
    REQUIRE(f.stopped());
}

TEST_CASE("HttpHostRfcTests: Upgrade in a response is a connection option") {
    HttpHostFixture f;
    f.addRoute(HttpMethod::get, "/r", [](HttpRequest &) -> Task<HttpResponse> {
        co_return HttpResponse{.status = HttpStatus::upgrade_required, .contentType = {}, .body = {}, .headers = {{"Upgrade", "websocket"}}};
    });

    SECTION("HTTP/1.1") {
        f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto resp = f.response();
        REQUIRE(resp.status == 426);
        REQUIRE(resp.headers.find("Connection")->second == "Upgrade");
        REQUIRE(!f.closed());
    }

    SECTION("with close") {
        f.receive("GET /r HTTP/1.0\r\n\r\n");
        REQUIRE(f.response().headers.find("Connection")->second == "Upgrade, close");
    }

    SECTION("with keep-alive") {
        f.receive("GET /r HTTP/1.0\r\nConnection: keep-alive\r\n\r\n");
        REQUIRE(f.response().headers.find("Connection")->second == "Upgrade, keep-alive");
    }
}

TEST_CASE("HttpHostRfcTests: 100 Continue before 101") {
    HttpHostFixture f;
    std::shared_ptr<IHttpUpgradedConnection> upgraded;
    auto handler = [&upgraded](HttpRequest &, IHttpResponseWriter &writer) -> Task<void> {
        HttpResponse response{.status = HttpStatus::switching_protocols, .contentType = {}, .body = {}, .headers = {{"Upgrade", "websocket"}}};
        co_await writer.start(std::move(response));
        auto connection = writer.upgrade();
        if(connection) {
            upgraded = *connection;
        }
    };
    f.addStreamingRoute(HttpMethod::get, "/chat", handler);
    f.addStreamingRoute(HttpMethod::post, "/chat", handler);

    SECTION("a request without content") {
        f.receive("GET /chat HTTP/1.1\r\nHost: example.org\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nExpect: 100-continue\r\n\r\n");
        REQUIRE(f.allSent().starts_with("HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 101 Switching Protocols\r\n"));
        REQUIRE(upgraded);
    }

    SECTION("only once when the content waited for it") {
        f.receive("POST /chat HTTP/1.1\r\nHost: example.org\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nExpect: 100-continue\r\nContent-Length: 2\r\n\r\n");
        REQUIRE(f.allSent() == "HTTP/1.1 100 Continue\r\n\r\n");
        f.receive("hi");
        auto const sent = f.allSent();
        REQUIRE(sent.starts_with("HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 101 Switching Protocols\r\n"));
        REQUIRE(sent.find("100 Continue", 20) == std::string::npos);
    }

    SECTION("not without Expect") {
        f.receive("GET /chat HTTP/1.1\r\nHost: example.org\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n\r\n");
        REQUIRE(f.allSent().starts_with("HTTP/1.1 101 Switching Protocols\r\n"));
    }
}

TEST_CASE("HttpHostRfcTests: an unknown method isn't added to the method names") {
    HttpHostFixture f;
    // The request is logged at trace level
    REQUIRE(f.logger.getService().getLogLevel() == LogLevel::LOG_TRACE);
    f.receive("FOO /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
    REQUIRE(f.response().status == 501);
    // Other threads read the map, adding to it would be a data race
    REQUIRE(ICHOR_REVERSE_METHOD_MATCHING.find(HttpMethod::unknown) == ICHOR_REVERSE_METHOD_MATCHING.end());
}

TEST_CASE("HttpHostRfcTests: an upgraded connection's receive handler is destroyed with the connection") {
    HttpHostFixture f;
    std::weak_ptr<IHttpUpgradedConnection> weak;
    std::string received;
    f.addStreamingRoute(HttpMethod::get, "/chat", [&weak, &received](HttpRequest &, IHttpResponseWriter &writer) -> Task<void> {
        HttpResponse response{.status = HttpStatus::switching_protocols, .contentType = {}, .body = {}, .headers = {{"Upgrade", "websocket"}}};
        co_await writer.start(std::move(response));
        auto connection = writer.upgrade();
        REQUIRE(connection);
        weak = *connection;
        // The handler holds on to the connection
        auto upgraded = *connection;
        upgraded->setReceiveHandler([upgraded, &received](std::span<uint8_t const> data) {
            received.append(reinterpret_cast<char const*>(data.data()), data.size());
            if(received.ends_with("bye")) {
                upgraded->close();
            }
        });
    });
    f.receive("GET /chat HTTP/1.1\r\nHost: example.org\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n\r\n");
    REQUIRE(f.response().status == 101);
    REQUIRE(!weak.expired());

    SECTION("closed by the handler, while it runs") {
        f.receive("hi bye");
        REQUIRE(received == "hi bye");
        REQUIRE(f.closed());
        REQUIRE(weak.expired());
    }

    SECTION("closed by the client") {
        f.removeDependency(*f.conn);
        REQUIRE(weak.expired());
    }
}

TEST_CASE("HttpHostRfcTests: the content of a streamed response that was replaced by a 500 isn't sent") {
    HttpHostFixture f;
    bool started{true};
    bool wrote{true};
    f.addStreamingRoute(HttpMethod::get, "/r", [&started, &wrote](HttpRequest &, IHttpResponseWriter &writer) -> Task<void> {
        HttpResponse response{.status = HttpStatus::ok, .contentType = {}, .body = toBody("first"), .headers = {{"X-Split", "a\r\nb"}}};
        started = static_cast<bool>(co_await writer.start(std::move(response)));
        wrote = static_cast<bool>(co_await writer.write(toBody("secret")));
    });
    f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\n\r\n");
    auto const all = f.allSent();
    auto const resp = parseResponse(all);
    REQUIRE(resp.status == 500);
    // The 500's own explanation, none of the handler's content
    REQUIRE(decodeChunked(resp.body) == "The server could not produce a valid response.\n");
    REQUIRE(!started);
    REQUIRE(!wrote);
}

TEST_CASE("HttpHostRfcTests: a 304 doesn't pass on the Content-Length a handler set for its content") {
    HttpHostFixture f;
    f.addRoute(HttpMethod::get, "/r", [](HttpRequest &) -> Task<HttpResponse> {
        co_return HttpResponse{.status = HttpStatus::ok, .contentType = {}, .body = toBody("content"), .headers = {{"ETag", "\"v1\""}, {"Content-Length", "7"}}};
    });
    f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\nIf-None-Match: \"v1\"\r\n\r\n");
    auto resp = f.response();
    REQUIRE(resp.status == 304);
    REQUIRE(!resp.headers.contains("Content-Length"));
}

TEST_CASE("HttpHostRfcTests: absolute-form OPTIONS without a path is OPTIONS *") {
    HttpHostFixture f;
    RecordedRequest req{};
    f.addRoute(HttpMethod::put, "/r", recordingHandler(req));
    f.receive("OPTIONS * HTTP/1.1\r\nHost: example.org\r\n\r\n");
    f.receive("OPTIONS http://example.org HTTP/1.1\r\nHost: example.org\r\n\r\n");
    // With a path it is about that path
    f.receive("OPTIONS http://example.org/ HTTP/1.1\r\nHost: example.org\r\n\r\n");
    auto resps = f.responses();
    REQUIRE(resps.size() == 3);
    REQUIRE(resps[1].status == 200);
    REQUIRE(resps[1].headers.find("Allow")->second == resps[0].headers.find("Allow")->second);
    REQUIRE(resps[2].status == 404);
}

TEST_CASE("HttpHostRfcTests: lingering connections are closed when the timer factory goes away") {
    HttpHostFixture f{timeouts(60'000, 60'000, 60'000), {}, true};
    RecordedRequest req{};
    f.addRoute(HttpMethod::get, "/r", recordingHandler(req));
    f.receive("GET /r HTTP/1.1\r\nHost: example.org\r\nConnection: close\r\n\r\n");
    REQUIRE(f.conn->getService().sendShutdown);
    REQUIRE(!f.stopped());
    // Nothing would end the lingering anymore
    f.removeDependency(*f.timerFactory);
    REQUIRE(f.stopped());
}

TEST_CASE("HttpHostRfcTests: extreme property values") {
    // Milliseconds that overflow the nanoseconds of a steady_clock time point
    constexpr uint64_t huge = 1ULL << 62U;
    auto props = timeouts(huge, huge, huge);
    props.emplace("MaxBodySize", Ichor::v1::make_any<uint64_t>(std::numeric_limits<uint64_t>::max()));
    HttpHostFixture f{std::move(props), {}, true};
    AsyncManualResetEvent mayFinish;
    f.addRoute(HttpMethod::get, "/slow", [&mayFinish](HttpRequest &) -> Task<HttpResponse> {
        co_await mayFinish;
        co_return HttpResponse{};
    });
    RecordedRequest req{};
    f.addRoute(HttpMethod::post, "/r", recordingHandler(req));

    SECTION("timeouts that don't fit are as good as never") {
        f.tick();
        REQUIRE(!f.closed());
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nContent-Length: 1\r\n\r\n");
        f.tick();
        REQUIRE(f.sent().empty());
        REQUIRE(!f.closed());
    }

    SECTION("MaxHeaderSize + MaxBodySize doesn't overflow") {
        f.receive("GET /slow HTTP/1.1\r\nHost: example.org\r\n\r\n");
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nContent-Length: 100000\r\n\r\n" + std::string(100'000, 'a'));
        REQUIRE(!f.closed());
        mayFinish.set();
        REQUIRE(f.responses().size() == 2);
        REQUIRE(req.body.size() == 100'000);
    }

    SECTION("a chunk size that would overflow") {
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nTransfer-Encoding: chunked\r\n\r\nffffffffffffffff\r\nab");
        REQUIRE(f.sent().empty());
        REQUIRE(!f.closed());
    }
}

TEST_CASE("HttpHostRfcTests: IP literals") {
    RecordedRequest req{};
    for(auto host : {"[::1]"sv, "[::]"sv, "[2001:db8::1]"sv, "[2001:db8:0:0:0:0:0:1]"sv, "[::ffff:192.0.2.1]"sv, "[1:2:3:4:5:6:7::]"sv, "[1:2:3:4:5:6:1.2.3.4]"sv,
                     "[v1.fe80::a+en1]"sv, "[::1]:8080"sv}) {
        HttpHostFixture f;
        f.addRoute(HttpMethod::get, "/r", recordingHandler(req));
        f.receive("GET /r HTTP/1.1\r\nHost: " + std::string{host} + "\r\n\r\n");
        INFO(host);
        REQUIRE(f.response().status == 200);
    }
    for(auto host : {"[zz]"sv, "[1:2:3:4:5:6:7:8:9]"sv, "[1::2::3]"sv, "[::ffff:256.0.0.1]"sv, "[::ffff:01.2.3.4]"sv, "[::1.2.3]"sv, "[12345::]"sv, "[1:2:3:4:5:6:7:8::]"sv,
                     "[:1]"sv, "[1:]"sv, "[1:2:3:4:5:6:7]"sv, "[v.x]"sv, "[v1.]"sv, "[vg.x]"sv, "[]"sv}) {
        HttpHostFixture f;
        f.addRoute(HttpMethod::get, "/r", recordingHandler(req));
        f.receive("GET /r HTTP/1.1\r\nHost: " + std::string{host} + "\r\n\r\n");
        INFO(host);
        REQUIRE(f.response().status == 400);
    }
}

TEST_CASE("HttpHostRfcTests: transfer coding parameters and syntax") {
    RecordedRequest req{};
    for(auto const &[transferEncoding, status] : {std::pair{"chunked;x=y"sv, 400U}, std::pair{"chunked ; x"sv, 400U}, std::pair{"chu\"nked"sv, 400U},
                                                 std::pair{"gzip;level=1, chunked"sv, 501U}, std::pair{"chunked, "sv, 200U}, std::pair{"CHUNKED"sv, 200U}}) {
        HttpHostFixture f;
        f.addRoute(HttpMethod::post, "/r", recordingHandler(req));
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nTransfer-Encoding: " + std::string{transferEncoding} + "\r\n\r\n0\r\n\r\n");
        INFO(transferEncoding);
        REQUIRE(f.response().status == status);
    }
}

TEST_CASE("HttpHostRfcTests: fields a handler has twice") {
    HttpHostFixture f;
    // Set-Cookie fields can't be combined into one (RFC 6265 3): cookies are separated by LF, which a field value can't have
    f.addRoute(HttpMethod::get, "/cookies", [](HttpRequest &) -> Task<HttpResponse> {
        co_return HttpResponse{.status = HttpStatus::ok, .contentType = {}, .body = {}, .headers = {{"Set-Cookie", "a=1; Path=/\nb=2; Expires=Wed, 21 Oct 2026 07:28:00 GMT"}}};
    });
    f.addRoute(HttpMethod::get, "/emptycookie", [](HttpRequest &) -> Task<HttpResponse> {
        co_return HttpResponse{.status = HttpStatus::ok, .contentType = {}, .body = {}, .headers = {{"Set-Cookie", "a=1\n\nb=2"}}};
    });
    f.addRoute(HttpMethod::get, "/lf", [](HttpRequest &) -> Task<HttpResponse> {
        co_return HttpResponse{.status = HttpStatus::ok, .contentType = {}, .body = {}, .headers = {{"X-A", "1\n2"}}};
    });
    // The host would add a second Content-Type
    f.addRoute(HttpMethod::get, "/both", [](HttpRequest &) -> Task<HttpResponse> {
        co_return HttpResponse{.status = HttpStatus::ok, .contentType = "text/plain", .body = {}, .headers = {{"content-type", "text/html"}}};
    });
    // No content, so no Content-Type from the host
    f.addRoute(HttpMethod::get, "/nocontent", [](HttpRequest &) -> Task<HttpResponse> {
        co_return HttpResponse{.status = HttpStatus::no_content, .contentType = "text/plain", .body = {}, .headers = {{"content-type", "text/html"}}};
    });

    SECTION("names ignore case, a field can't be there twice") {
        HttpResponse response{};
        response.headers.emplace("Cache-Control", "no-store");
        REQUIRE(!response.headers.emplace("cache-control", "no-cache").second);
        REQUIRE(response.headers.size() == 1);
    }

    SECTION("each cookie is its own Set-Cookie field") {
        f.receive("GET /cookies HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto const sent = f.allSent();
        REQUIRE(sent.starts_with("HTTP/1.1 200 OK\r\n"));
        REQUIRE(sent.find("\r\nSet-Cookie: a=1; Path=/\r\n") != std::string::npos);
        REQUIRE(sent.find("\r\nSet-Cookie: b=2; Expires=Wed, 21 Oct 2026 07:28:00 GMT\r\n") != std::string::npos);
    }

    SECTION("an empty cookie, or LF in another field, is 500") {
        f.receive("GET /emptycookie HTTP/1.1\r\nHost: example.org\r\n\r\nGET /lf HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto resps = f.responses();
        REQUIRE(resps.size() == 2);
        REQUIRE(resps[0].status == 500);
        REQUIRE(resps[1].status == 500);
    }

    SECTION("contentType and a Content-Type field is 500") {
        f.receive("GET /both HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.response().status == 500);
    }

    SECTION("only one of them sent is fine") {
        f.receive("GET /nocontent HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.response().status == 204);
    }
}

TEST_CASE("HttpHostRfcTests: Expect is a list") {
    RecordedRequest req{};
    for(auto const &[expect, continues] : {std::pair{"100-continue, "sv, true}, std::pair{", 100-Continue"sv, true}, std::pair{"100-continue,100-continue"sv, true}}) {
        HttpHostFixture f;
        f.addRoute(HttpMethod::post, "/r", recordingHandler(req));
        f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nExpect: " + std::string{expect} + "\r\nContent-Length: 2\r\n\r\n");
        INFO(expect);
        REQUIRE(f.allSent() == (continues ? "HTTP/1.1 100 Continue\r\n\r\n" : ""));
    }
    HttpHostFixture f;
    f.addRoute(HttpMethod::post, "/r", recordingHandler(req));
    f.receive("POST /r HTTP/1.1\r\nHost: example.org\r\nExpect: 100-continue, something-else\r\nContent-Length: 2\r\n\r\n");
    REQUIRE(f.response().status == 417);
}

TEST_CASE("HttpHostRfcTests: an https target over a plain connection is 421") {
    RecordedRequest req{};
    for(auto const scheme : {"https"sv, "HTTPS"sv}) {
        HttpHostFixture f;
        f.addRoute(HttpMethod::get, "/r", recordingHandler(req));
        f.receive("GET " + std::string{scheme} + "://example.org/r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        INFO(scheme);
        REQUIRE(f.response().status == 421);
        REQUIRE(f.closed());
    }

    SECTION("before the content, without asking for it") {
        HttpHostFixture f;
        f.addRoute(HttpMethod::post, "/r", recordingHandler(req));
        f.receive("POST https://example.org/r HTTP/1.1\r\nHost: example.org\r\nExpect: 100-continue\r\nContent-Length: 5\r\n\r\n");
        REQUIRE(f.response().status == 421);
    }

    SECTION("http is fine") {
        HttpHostFixture f;
        f.addRoute(HttpMethod::get, "/r", recordingHandler(req));
        f.receive("GET http://example.org/r HTTP/1.1\r\nHost: example.org\r\n\r\n");
        REQUIRE(f.response().status == 200);
    }
}

TEST_CASE("HttpHostRfcTests: CONNECT to an invalid port is 400") {
    for(auto const &[authority, status] : {std::pair{"example.org:0"sv, 400U}, std::pair{"example.org:65536"sv, 400U}, std::pair{"example.org:99999"sv, 400U},
                                           std::pair{"example.org:123456"sv, 400U}, std::pair{"example.org:65535"sv, 404U}, std::pair{"[::1]:1"sv, 404U},
                                           std::pair{"example.org:000443"sv, 404U}, std::pair{"example.org:00000000000000000000065536"sv, 400U}}) {
        HttpHostFixture f;
        f.receive("CONNECT " + std::string{authority} + " HTTP/1.1\r\nHost: " + std::string{authority} + "\r\n\r\n");
        INFO(authority);
        REQUIRE(f.response().status == status);
    }
}

TEST_CASE("HttpHostRfcTests: the errors the host responds with explain themselves") {
    // RFC 9110 15.5, 15.6: what the error is and whether it's temporary, 15.6.6: what versions are supported
    for(auto const &[request, status, explanation] : {std::tuple{"GET /r HTTP/1.1\r\n\r\n"sv, 400U, "The request is malformed or ambiguous, sending it again unchanged won't help.\n"sv},
                                                      std::tuple{"GET /r HTTP/2.0\r\nHost: example.org\r\n\r\n"sv, 505U, "This server supports HTTP/1.x.\n"sv},
                                                      std::tuple{"FOO /r HTTP/1.1\r\nHost: example.org\r\n\r\n"sv, 501U, "This server does not implement the request's method or transfer coding.\n"sv},
                                                      std::tuple{"DELETE /r HTTP/1.1\r\nHost: example.org\r\n\r\n"sv, 405U, "This method is not allowed for this target, the Allow field lists the ones that are.\n"sv}}) {
        HttpHostFixture f;
        RecordedRequest req{};
        f.addRoute(HttpMethod::get, "/r", recordingHandler(req));
        f.receive(request);
        INFO(request);
        auto resp = f.response();
        REQUIRE(resp.status == status);
        REQUIRE(resp.headers.find("Content-Type")->second == "text/plain; charset=utf-8");
        REQUIRE(resp.body == explanation);
    }

    // Not for HEAD, which has the fields of the GET response but no content
    HttpHostFixture f;
    f.receive("HEAD /nothing HTTP/1.1\r\nHost: example.org\r\n\r\n");
    auto resp = f.response();
    REQUIRE(resp.status == 404);
    REQUIRE(resp.body.empty());
    REQUIRE(resp.headers.find("Content-Length")->second == "33");
}

TEST_CASE("HttpHostRfcTests: errors to HEAD requests have no content") {
    // The method is known before the error is: the request line was read
    for(auto const &[request, status] : {std::pair{"HEAD /r HTTP/2.0\r\nHost: example.org\r\n\r\n"sv, 505U}, std::pair{"HEAD /r HTTP/1.1\r\n\r\n"sv, 400U},
                                         std::pair{"HEAD /r HTTP/1.1\r\nHost: example.org\r\nExpect: something\r\n\r\n"sv, 417U},
                                         std::pair{"HEAD https://example.org/r HTTP/1.1\r\nHost: example.org\r\n\r\n"sv, 421U}}) {
        HttpHostFixture f;
        f.receive(request);
        INFO(request);
        auto resp = f.response();
        REQUIRE(resp.status == status);
        REQUIRE(resp.body.empty());
    }

    SECTION("a HEAD request that takes too long") {
        HttpHostFixture f{timeouts(60'000, 5, 60'000), {}, true};
        f.receive("HEAD /r HTTP/1.1\r\nHost: exa");
        std::this_thread::sleep_for(20ms);
        f.tick();
        auto resp = f.response();
        REQUIRE(resp.status == 408);
        REQUIRE(resp.body.empty());
    }
}
