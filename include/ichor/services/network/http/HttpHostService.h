#pragma once

#include <ichor/services/network/http/IHttpHostService.h>
#include <ichor/services/network/http/HttpInternal.h>
#include <ichor/services/network/IHostService.h>
#include <ichor/services/network/IConnectionService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/event_queues/IEventQueue.h>
#include <tl/expected.h>

namespace Ichor {
    /**
     * Service for creating an HTTP/1.1 server using boost. Requires an IAsioContextService and a logger.
     *
     * Properties:
     * - "Address" std::string - What address to bind to (required)
     * - "Port" uint16_t - What port to bind to (required)
     * - "Priority" uint64_t - What priority to insert events with (e.g. when getting a response from the client)
     * - "NoDelay" bool - whether to enable TCP nodelay, a.k.a. disabling Nagle's algorithm, for reduced latency at the expense of throughput. (default: false)
     * - "ConnectOverSsl" bool - Set to true to connect over HTTPS instead of HTTP (default: false)
     * - "RootCA" std::string - If ConnectOverSsl is true and this property is set, trust the given RootCA (default: not set)
     * - "TryConnectIntervalMs" uint64_t - with which interval in milliseconds to try (re)connecting (default: 100 ms)
     * - "TimeoutMs" uint64_t - with which interval in milliseconds to timeout for (re)connecting, after which the service stops itself (default: 10'000 ms)
     * - "Debug" bool - Enable verbose logging of requests and responses (default: false)
     */
    class HttpHostService final : public IHttpHostService, public AdvancedService<HttpHostService> {
    public:
        HttpHostService(DependencyRegister &reg, Properties props);
        ~HttpHostService() final = default;

        HttpRouteRegistration addRoute(HttpMethod method, std::string_view route, std::function<Task<HttpResponse>(HttpRequest&)> handler) final;
        HttpRouteRegistration addRoute(HttpMethod method, std::unique_ptr<RouteMatcher> matcher, std::function<Task<HttpResponse>(HttpRequest&)> handler) final;
        void removeRoute(HttpMethod method, RouteIdType id) final;

        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(ILogger &logger, IService &isvc);
        void removeDependencyInstance(ILogger &logger, IService &isvc);

        void addDependencyInstance(IEventQueue &q, IService &isvc);
        void removeDependencyInstance(IEventQueue &q, IService &isvc);

        void addDependencyInstance(IHostService &c, IService &isvc);
        void removeDependencyInstance(IHostService &c, IService &isvc);

        void addDependencyInstance(IHostConnectionService &c, IService &isvc);
        void removeDependencyInstance(IHostConnectionService &c, IService &isvc);

        tl::expected<HttpRequest, HttpParseError> parseRequest(std::string_view complete, size_t& len) const;
        Task<void> receiveRequestHandler(ServiceIdType id);
        Task<void> sendResponse(ServiceIdType id, const HttpResponse &response);

        friend DependencyRegister;

        uint64_t _priority{INTERNAL_EVENT_PRIORITY};
        uint64_t _streamIdCounter{};
        uint64_t _matchersIdCounter{};
        bool _sendServerHeader{true};
        bool _debug{};
        unordered_map<HttpMethod, unordered_map<std::unique_ptr<RouteMatcher>, std::function<Task<HttpResponse>(HttpRequest&)>>> _handlers{};
        ILogger* _logger{};
        IEventQueue *_queue;
        unordered_set<ServiceIdType> _hostServiceIds;
        unordered_map<ServiceIdType, IHostConnectionService*> _connections;
        unordered_map<ServiceIdType, std::string> _connectionBuffers;
    };
}
