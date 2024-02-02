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
        _routeRegistrations.emplace_back(host->addRoute(HttpMethod::post, "/test", [this](HttpRequest &req) -> AsyncGenerator<HttpResponse> {
            co_return HttpResponse{HttpStatus::ok, "application/text", "<html><body>This is my basic webpage</body></html>", {}};
        }));
    }
    std::vector<std::unique_ptr<HttpRouteRegistration>> _routeRegistrations{};
};
```
