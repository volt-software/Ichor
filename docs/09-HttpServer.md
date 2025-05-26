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
    auto &dm = queue->createManager();
    dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>();
    dm.createServiceManager<AsioContextService, IAsioContextService>(Properties{{"Threads", Ichor::make_any<uint64_t>(2)}});
    dm.createServiceManager<HttpHostService, IHttpHostService>(Properties{{"Address", Ichor::make_any<std::string>("localhost")}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}});
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
