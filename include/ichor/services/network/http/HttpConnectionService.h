#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <ichor/coroutines/AsyncReturningManualResetEvent.h>
#include <ichor/services/network/http/IHttpConnectionService.h>
#include <ichor/services/network/http/HttpInternal.h>
#include <ichor/services/network/IConnectionService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/event_queues/IEventQueue.h>
#include <tl/expected.h>
#include <deque>

namespace Ichor {
    /**
     * Service for creating an HTTP/1.1 server. Does not support chunked encoding and is technically not RFC 7320 compliant.
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
    class HttpConnectionService final : public IHttpConnectionService, public AdvancedService<HttpConnectionService> {
    public:
        HttpConnectionService(DependencyRegister &reg, Properties props);
        ~HttpConnectionService() final = default;

        Task<HttpResponse> sendAsync(HttpMethod method, std::string_view route, unordered_map<std::string, std::string> &&headers, std::vector<uint8_t>&& msg) final;

        Task<void> close() final;

        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(ILogger &logger, IService &isvc);
        void removeDependencyInstance(ILogger &logger, IService &isvc);

        void addDependencyInstance(IEventQueue &q, IService &isvc);
        void removeDependencyInstance(IEventQueue &q, IService &isvc);

        void addDependencyInstance(IClientConnectionService &c, IService &isvc);
        void removeDependencyInstance(IClientConnectionService &c, IService &isvc);

        tl::expected<HttpResponse, HttpParseError> parseResponse(std::string_view complete, size_t& len) const;

        friend DependencyRegister;

        uint64_t _priority{INTERNAL_EVENT_PRIORITY};
        bool _debug{};
        ILogger* _logger{};
        IEventQueue *_queue{};
        IClientConnectionService *_connection{};
        std::deque<AsyncReturningManualResetEvent<tl::expected<HttpResponse, HttpParseError>>> _events;
        std::string _buffer;
    };
}
