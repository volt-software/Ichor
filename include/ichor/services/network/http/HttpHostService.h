#pragma once

#include <ichor/services/network/http/IHttpHostService.h>
#include <ichor/services/network/http/HttpInternal.h>
#include <ichor/services/network/http/HttpRequestParser.h>
#include <ichor/services/network/IHostService.h>
#include <ichor/services/network/IConnectionService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/event_queues/IEventQueue.h>
#include <tl/expected.h>
#include <ichor/ScopedServiceProxy.h>
#include <chrono>

namespace Ichor::v1 {
    namespace Detail {
        // NOLINTNEXTLINE(cppcoreguidelines-virtual-class-destructor): final, and IHttpResponseWriter's destructor is protected: nothing deletes it through another type
        class HttpHostResponseWriter;
        class HttpHostUpgradedConnection;
    }

    /**
     * HTTP/1.1 server (RFC 9110 and RFC 9112) on top of an IHostService and the IHostConnectionServices it creates, e.g. TcpHostService.
     *
     * - Requests on a connection are handled one at a time, in the order they arrive, also when they're pipelined.
     * - Requests that can't be parsed are answered with 400 (or 414, 431, 413, 417, 501 or 505) after which the connection is closed.
     * - HEAD requests without a HEAD route use the GET route, without sending its body. OPTIONS requests without an OPTIONS route, and requests with a method
     *   that only has routes for other methods, are answered with the Allow header (200 and 405 respectively).
     * - Responses always get a Date and the framing (Content-Length) is done by this service, handlers don't set Content-Length or Transfer-Encoding.
     *   Handlers can set "Connection: close" to close the connection after the response.
     * - The responses of GET and HEAD routes with an ETag or Last-Modified header are turned into 304 Not Modified or 412 Precondition Failed
     *   according to If-None-Match, If-Modified-Since, If-Match and If-Unmodified-Since. Routes for other methods have to evaluate those themselves.
     * - Streaming routes (addStreamingRoute) send their content chunked, or close-delimited to HTTP/1.0 clients, and can take a connection over
     *   after a 101 Switching Protocols or a 2xx response to CONNECT.
     * - With an ITimerFactory (an optional dependency), idle connections are closed, requests that take too long to arrive are answered with
     *   408 Request Timeout, and connections are closed gracefully: the sending side first, then waiting for the client to close, so that a client
     *   that is still sending doesn't lose the last response to a reset (RFC 9112 9.6). Without one, connections are closed right away.
     *
     * Properties:
     * - "Address" std::string - What address to bind to (required)
     * - "Port" uint16_t - What port to bind to (required)
     * - "Priority" uint64_t - What priority to insert events with (e.g. when getting a response from the client)
     * - "SendServerHeader" bool - Send "Server: Ichor" with each response. Off by default, it tells an attacker what software to target (default: false)
     * - "MaxRequestLineSize" uint64_t - Requests with a longer request line are answered with 414 (default: 8 KiB)
     * - "MaxHeaderSize" uint64_t - Requests with a larger header section are answered with 431, chunked bodies whose framing is larger than this plus
     *   their content with 400 (default: 64 KiB)
     * - "MaxBodySize" uint64_t - Requests with a larger body are answered with 413 (default: 512 MiB)
     * - "IdleTimeoutMs" uint64_t - Close connections without a request for this long, 0 to never (default: 60'000, needs an ITimerFactory)
     * - "RequestTimeoutMs" uint64_t - Answer requests that aren't complete this long after they started with 408, 0 to never (default: 30'000, needs an ITimerFactory)
     * - "LingerTimeoutMs" uint64_t - When closing, how long to wait for the client to close, 0 to close right away (default: 2'000, needs an ITimerFactory)
     * - "EnableTrace" bool - Answer TRACE requests without a TRACE route by reflecting the request, without Authorization, Proxy-Authorization and
     *   Cookie (RFC 9110 9.3.8). Off by default: reflecting requests can expose what a browser would otherwise hide from scripts (default: false)
     * - "Debug" bool - Enable verbose logging of requests and responses (default: false)
     */
    class HttpHostService final : public IHttpHostService, public AdvancedService<HttpHostService> {
    public:
        HttpHostService(DependencyRegister &reg, Properties props);
        ~HttpHostService() final = default;

        HttpRouteRegistration addRoute(HttpMethod method, std::string_view route, std::function<Task<HttpResponse>(HttpRequest&)> handler) final;
        HttpRouteRegistration addRoute(HttpMethod method, std::unique_ptr<RouteMatcher> matcher, std::function<Task<HttpResponse>(HttpRequest&)> handler) final;
        using IHttpHostService::addStreamingRoute;
        HttpRouteRegistration addStreamingRoute(HttpMethod method, std::unique_ptr<RouteMatcher> matcher, HttpStreamingHandler handler) final;
        void removeRoute(HttpMethod method, RouteIdType id) final;

        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        enum class ConnectionPhase : uint_fast8_t {
            /// Waiting for a request
            IDLE,
            /// Receiving a request
            REQUEST,
            /// A handler is responding to a request
            PROCESSING,
            /// Closed for sending, waiting for the client to close
            LINGERING,
            /// Handed over to a handler, no longer HTTP
            UPGRADED,
        };

        struct ConnectionState {
            /// Received, from bufferStart on not yet handled
            std::string buffer;
            /// What was handled is only removed from the buffer now and then, removing every pipelined request would copy what follows it each time
            size_t bufferStart{};
            /// Of the request at bufferStart
            Detail::HttpRequestParseState parse;
            ConnectionPhase phase{ConnectionPhase::IDLE};
            /// When the phase times out
            std::chrono::steady_clock::time_point deadline{};
            /// A receiveRequestHandler is handling this connection's requests
            bool processing{};
            /// The connection is being closed, what it still receives is dropped
            bool closing{};
            /// A 100 Continue was sent for the request at the start of the buffer
            bool sentContinue{};
            /// The connection service has been asked to stop
            bool stopRequested{};
            std::shared_ptr<Detail::HttpHostUpgradedConnection> upgraded;

            /// Received and not yet handled
            [[nodiscard]] std::string_view pending() const noexcept {
                return std::string_view{buffer}.substr(bufferStart);
            }

            /// The request at the start of pending() was handled
            void consume(size_t size) {
                bufferStart += size;
                parse = {};
                // Copies at most as much as was handled since the last time
                if(bufferStart * 2 >= buffer.size()) {
                    buffer.erase(0, bufferStart);
                    bufferStart = 0;
                }
            }

            void clearBuffer() noexcept {
                buffer.clear();
                bufferStart = 0;
                parse = {};
            }
        };

        struct RouteHandler {
            std::function<Task<HttpResponse>(HttpRequest&)> handler;
            /// Instead of handler, for streaming routes
            HttpStreamingHandler streamingHandler;
        };

        /// What a response is a response to
        struct ResponseContext {
            HttpMethod method{HttpMethod::get};
            /// For the Allow header of a 405
            std::string_view target;
            uint8_t minorVersion{1};
            HttpHeaders const *requestHeaders{};
            /// The client waits for a 100 Continue that wasn't sent, which has to come before a 101 (RFC 9110 7.8)
            bool owesContinue{};
        };

        enum class ResponseFraming : uint_fast8_t {
            CONTENT_LENGTH,
            CHUNKED,
            /// The content ends when the connection closes
            CLOSE_DELIMITED,
            /// No content: 1xx, 204, 304, HEAD, a tunnel
            NONE,
        };

        struct SerializedResponse {
            std::vector<uint8_t> msg;
            ResponseFraming framing{ResponseFraming::NONE};
            /// Whether the connection has to be closed after the response
            bool close{};
            /// The handler's response was invalid and was replaced by a 500
            bool replaced{};
        };

        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService &isvc);
        void removeDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService &isvc);

        void addDependencyInstance(Ichor::ScopedServiceProxy<IEventQueue*> q, IService &isvc);
        void removeDependencyInstance(Ichor::ScopedServiceProxy<IEventQueue*> q, IService &isvc);

        void addDependencyInstance(Ichor::ScopedServiceProxy<IHostService*> c, IService &isvc);
        void removeDependencyInstance(Ichor::ScopedServiceProxy<IHostService*> c, IService &isvc);

        void addDependencyInstance(Ichor::ScopedServiceProxy<IHostConnectionService*> c, IService &isvc);
        void removeDependencyInstance(Ichor::ScopedServiceProxy<IHostConnectionService*> c, IService &isvc);

        void addDependencyInstance(Ichor::ScopedServiceProxy<ITimerFactory*> timerFactory, IService &isvc);
        void removeDependencyInstance(Ichor::ScopedServiceProxy<ITimerFactory*> timerFactory, IService &isvc);

        void receive(ServiceIdType id, std::span<uint8_t const> data);
        Task<void> receiveRequestHandler(ServiceIdType id);
        /// Routes the request and sends the response, returns whether the connection has to be closed afterwards
        Task<bool> handleRequest(ServiceIdType id, Detail::ParsedHttpRequest &request, bool sentContinue);
        /// The methods with a route matching route, plus HEAD for GET and OPTIONS. Empty if no route matches.
        std::vector<HttpMethod> allowedMethods(std::string_view route);
        /// The status line and header fields, followed by the content if the length is known (not streaming)
        SerializedResponse serializeResponse(HttpResponse &response, ResponseContext const &context, bool streaming, bool close);
        /// Serializes and sends a complete response, returns whether the connection has to be closed afterwards
        Task<bool> sendResponse(ServiceIdType id, HttpResponse &response, ResponseContext const &context, bool keepAlive);
        /// Whether a 101 response switches to protocols the request asked for (RFC 9110 7.8)
        [[nodiscard]] bool canSwitchProtocols(HttpResponse const &response, ResponseContext const &context) const;
        Task<bool> send(ServiceIdType id, std::vector<uint8_t> &&msg);
        /// Closes gracefully if there's a timer for the linger timeout
        void closeConnection(ServiceIdType id);
        /// Stops the connection service, which closes the connection right away
        void stopConnection(ServiceIdType id);
        void setIdleOrRequestPhase(ConnectionState &state);
        void startTimeoutTimer();
        void stopTimeoutTimer();
        void checkTimeouts();

        friend DependencyRegister;
        friend Detail::HttpHostResponseWriter;
        friend Detail::HttpHostUpgradedConnection;

        uint64_t _priority{INTERNAL_EVENT_PRIORITY};
        uint64_t _matchersIdCounter{};
        bool _sendServerHeader{};
        bool _enableTrace{};
        bool _debug{};
        bool _started{};
        Detail::HttpRequestLimits _limits{};
        std::chrono::milliseconds _idleTimeout{60'000};
        std::chrono::milliseconds _requestTimeout{30'000};
        std::chrono::milliseconds _lingerTimeout{2'000};
        unordered_map<HttpMethod, unordered_map<std::unique_ptr<RouteMatcher>, RouteHandler>> _handlers{};
        Ichor::ScopedServiceProxy<ILogger*> _logger {};
        Ichor::ScopedServiceProxy<IEventQueue*> _queue ;
        Ichor::ScopedServiceProxy<ITimerFactory*> _timerFactory {};
        tl::optional<TimerRef> _timeoutTimer{};
        unordered_set<ServiceIdType, ServiceIdHash> _hostServiceIds;
        unordered_map<ServiceIdType, Ichor::ScopedServiceProxy<IHostConnectionService*>, ServiceIdHash> _connections;
        unordered_map<ServiceIdType, ConnectionState, ServiceIdHash> _connectionStates;
    };
}
