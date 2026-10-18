# Http Server

Ichor provides an Expressjs-inspired abstraction for setting up an Http(s) server.

## Creating Host

The Boost HttpHostService implementation requires a logger, the AsioContextService and the HttpHostService itself to be set up:

```c++
#include <ichor/services/network/boost/HttpHostService.h>
#include <ichor/services/network/boost/AsioContextService.h>
#include <ichor/services/logging/NullLogger.h>


int main(int argc, char *argv[]) {
    auto queue = std::make_unique<PriorityQueue>();
    auto &dm = queue->getManager();
    dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>();
    dm.createServiceManager<AsioContextService, IAsioContextService>(Properties{{"Threads", Ichor::v1::make_any<uint64_t>(2)}});
    dm.createServiceManager<HttpHostService, IHttpHostService>(Properties{{"Address", Ichor::v1::make_any<std::string>("localhost")}, {"Port", Ichor::v1::make_any<uint16_t>(static_cast<uint16_t>(8001))}});
    queue->start(CaptureSigInt);
    return 0;
}
```

## Adding routes

```c++
#include <ichor/services/network/http/IHttpHostService.h>

class UsingHttpService final {
    UsingHttpService(IHttpHostService *host) {
        _routeRegistrations.emplace_back(host->addRoute(HttpMethod::post, "/test", [this](HttpRequest &req) -> Task<HttpResponse> {
            std::string_view body_view = "<html><body>This is my basic webpage</body></html>";
            std::vector<uint8_t> body{body_view.begin(), body_view.end()};
            co_return HttpResponse{HttpStatus::ok, "text/plain", std::move(body), {}};
        }));
    }
    std::vector<HttpRouteRegistration> _routeRegistrations{};
};
```

## Using regex in routes

To add routes that capture parts of the URL, Ichor provides a regex route matcher that supports capture groups:

```c++
#include <ichor/services/network/http/IHttpHostService.h>

class UsingHttpService final {
    UsingHttpService(IHttpHostService *host) {
        _routeRegistrations.emplace_back(svc.addRoute(HttpMethod::get, std::make_unique<RegexRouteMatch<R"(\/user\/(\d{1,2})\?*(.*))">>(), [this](HttpRequest &req) -> Task<HttpResponse> {
            std::string user_id = req.regex_params[0];
            if(req.regex_params.size() > 1) {
                std::string query_params = req.regex_params[1]; // e.g. param1=one&param2=two, parsing string is left to the user for now, though Ichor does provide a string_view split function in stl/StringUtils.h
            }
            co_return HttpResponse{HttpStatus::ok, "text/plain", {}, {}};
        }));
    }
    std::vector<HttpRouteRegistration> _routeRegistrations{};
};
```

The `regex_params` is a vector with the capture groups counted from left to right.

## Custom route matcher

If you want to add custom logic on matching routes, because maybe you want to use a different regex library than CTRE, implement the interface to RouteMatcher:

```c++
struct CustomRouteMatcher final : public RouteMatcher {
    ~CustomRouteMatcher() noexcept final = default;

    [[nodiscard]] bool matches(std::string_view route) noexcept final {
        if(route == "/test") {
            return true;
        }

        return false;
    }

    // these get moved into HttpRequest's regex_params
    [[nodiscard]] std::vector<std::string> route_params() noexcept final {
        return {};
    }
};
```

And use that with the route registration:

```c++
_routeRegistrations.emplace_back(svc.addRoute(HttpMethod::get, std::make_unique<CustomRouteMatcher>(), [this](HttpRequest &req) -> Task<HttpResponse> {
    co_return HttpResponse{HttpStatus::ok, "text/plain", {}, {}};
}));
```

## HTTP/1.1 behaviour

See [HTTP/1.1 conformance](11-HttpConformance.md) for each requirement of the RFCs and how it is met.

Ichor's own `Ichor::v1::HttpHostService` (`ichor/services/network/http/HttpHostService.h`, on top of a TCP host service) implements the server side of [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110) (HTTP semantics) and [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112) (HTTP/1.1). What that means for routes:

* `HttpRequest::route`, `address` and the request's other views are only valid while the handler runs. Copy what you need to keep.
* Header names are case-insensitive: `req.headers.find("content-type")` finds `Content-Type`. Field lines with the same name are combined into one value, separated by `, ` (`; ` for `Cookie`).
* Request bodies can use `Content-Length` or `Transfer-Encoding: chunked`. The body a handler gets is the content without the transfer coding, followed by a null terminator that isn't part of the content. Chunk extensions and trailer fields are discarded. The chunk framing (chunk-size lines, extensions and CRLFs) can be at most the size of the content plus `MaxHeaderSize`, so even one byte chunks fit, but endless extensions don't. `chunked` with parameters is `400`.
* Requests on a connection are handled one at a time, in order, including pipelined ones. A request with `Expect: 100-continue` gets a `100 Continue` before its body is read.
* `HEAD` requests without a `HEAD` route use the `GET` route, with `req.method` set to `HttpMethod::head`, and get its headers and `Content-Length` without the content.
* A method without a route for a resource that has routes for other methods is answered with `405 Method Not Allowed` and an `Allow` header. `OPTIONS` without an `OPTIONS` route is answered with `200` and `Allow`, `OPTIONS *` with the methods of the whole server. Methods Ichor doesn't know get `501 Not Implemented`.
* The framing of a response is done by the host: handlers don't set `Content-Length` or `Transfer-Encoding`. A `HEAD` route or a `304` can set `Content-Length` to what a `GET` response would have had. `204` and `304` responses never have content.
* Each response gets a `Date` header unless the handler sets one. There is no `Server` header unless the `SendServerHeader` property is set, as it tells an attacker what software to target.
* A handler can set `Connection: close` to close the connection after its response. HTTP/1.0 connections are closed after each response, unless the client asks for `keep-alive`.
* A `GET` or `HEAD` route that responds with an `ETag` or `Last-Modified` header gets conditional requests for free: `If-None-Match` and `If-Modified-Since` turn its 2xx response into `304 Not Modified`, `If-Match` and `If-Unmodified-Since` into `412 Precondition Failed`. Routes for other methods have to evaluate preconditions themselves, before they change anything.
* A handler that responds with an invalid status (anything outside 200-599, except a `101` from a streaming route, see below) or a header that isn't a valid field line gets a `500 Internal Server Error` instead, so a handler can't split a response by putting a CRLF in a header.
* Invalid requests are answered with `400 Bad Request`, after which the connection is closed: the next request can't be found reliably. So are requests with both `Content-Length` and `Transfer-Encoding`, a missing or duplicate `Host`, obsolete line folding, a bare CR or LF, or whitespace before a field's colon, as those are what request smuggling uses. Other errors that close the connection are `414 URI Too Long`, `431 Request Header Fields Too Large` and `413 Content Too Large` (see the `MaxRequestLineSize`, `MaxHeaderSize` and `MaxBodySize` properties), `417 Expectation Failed`, `501 Not Implemented` for transfer codings other than `chunked` and `505 HTTP Version Not Supported`.

## Timeouts and closing connections

With an `ITimerFactory` available (e.g. a `TimerFactoryFactory` or `IOUringTimerFactoryFactory` registered in the `DependencyManager`), the host:

* closes connections that have been idle for `IdleTimeoutMs` (default 60 s),
* answers requests that haven't completely arrived `RequestTimeoutMs` after their first byte with `408 Request Timeout` (default 30 s). That's measured from the start of the request, so a client sending a byte at a time can't keep a connection busy,
* closes connections gracefully: it closes its sending side first, so the client sees the end of the response, and keeps reading (and dropping) what the client still sends until the client closes or `LingerTimeoutMs` passes (default 2 s). Closing right away with unread data makes the TCP stack reset the connection, which can make the client lose the last response (RFC 9112 9.6).

A timeout of 0 disables it. Without an `ITimerFactory`, there are no timeouts and connections are closed right away.

## Streaming responses

A streaming route sends its response in parts, e.g. for server-sent events or large generated content:

```c++
_routeRegistrations.emplace_back(host->addStreamingRoute(HttpMethod::get, "/events", [](HttpRequest &req, IHttpResponseWriter &writer) -> Task<void> {
    // A named response: gcc 12 destroys an aggregate temporary in a co_await expression twice
    HttpResponse response{HttpStatus::ok, "text/event-stream", {}, {}};
    if(!co_await writer.start(std::move(response))) {
        co_return; // the client is gone, or the response was invalid and replaced by a 500
    }
    for(int i = 0; i < 3; i++) {
        std::string_view event = "data: tick\n\n";
        if(!co_await writer.write(std::vector<uint8_t>{event.begin(), event.end()})) {
            co_return; // the client is gone
        }
    }
}));
```

The content is sent with `Transfer-Encoding: chunked` to HTTP/1.1 clients, and ends by closing the connection for HTTP/1.0 clients. The response is complete when the handler returns. The writer is only valid while the handler runs. Hosts that can't stream (the Boost.BEAST one) send the response at once when the handler returns.

## Upgrading connections and CONNECT

A streaming route can take a connection over after a `101 Switching Protocols` response to a request that asked for that protocol in its `Upgrade` header, or after a 2xx response to `CONNECT` (a tunnel):

```c++
_routeRegistrations.emplace_back(host->addStreamingRoute(HttpMethod::get, "/chat", [this](HttpRequest &req, IHttpResponseWriter &writer) -> Task<void> {
    HttpResponse response{HttpStatus::switching_protocols, {}, {}, {{"Upgrade", "my-protocol"}}};
    co_await writer.start(std::move(response));
    auto connection = writer.upgrade();
    if(!connection) {
        co_return;
    }
    _connection = *connection; // a std::shared_ptr<IHttpUpgradedConnection>
    _connection->setReceiveHandler([this](std::span<uint8_t const> data) {
        // what the client sends, starting with what it sent after the request
    });
}));
```

The host checks that the client asked for the protocol (`Upgrade` and `Connection: upgrade` in an HTTP/1.1 request) and answers with `500` otherwise. A client that sent `Expect: 100-continue` gets a `100 Continue` before the `101`. The receive handler is destroyed when the connection closes, so it can hold on to the connection without leaking it. After `upgrade()`, the host no longer reads HTTP from the connection: everything goes to the receive handler, and `sendAsync()` sends raw data. A `101` without `upgrade()` closes the connection. Not supported: compressing responses (`Content-Encoding` is up to the handler). Use the Boost.BEAST services for websockets, which implement the websocket protocol itself.
