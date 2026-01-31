#pragma once

#include <ichor/event_queues/BoostAsioQueue.h>
#include <ichor/services/network/http/IHttpHostService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/circular_buffer.hpp>
#include <memory>
#include <ichor/ScopedServiceProxy.h>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace Ichor::Boost::v1 {
    namespace Detail {
        using HostOutboxMessage = http::response<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>>;

        template <typename SocketT>
        struct Connection {
            Connection(tcp::socket &&_socket) : socket(std::move(_socket)) {}
            Connection(tcp::socket &&_socket, net::ssl::context &ctx) : socket(std::move(_socket), ctx) {}
            SocketT socket;
            boost::circular_buffer<HostOutboxMessage> outbox{10};
        };
    }

    /**
     * Service for creating an HTTP/1.1 server using boost. Requires an IBoostAsioQueue and a logger.
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
    class HttpHostService final : public Ichor::v1::IHttpHostService, public AdvancedService<HttpHostService> {
    public:
        HttpHostService(DependencyRegister &reg, Properties props);
        ~HttpHostService() final = default;

        Ichor::v1::HttpRouteRegistration addRoute(Ichor::v1::HttpMethod method, std::string_view route, std::function<Task<Ichor::v1::HttpResponse>(Ichor::v1::HttpRequest&)> handler) final;
        Ichor::v1::HttpRouteRegistration addRoute(Ichor::v1::HttpMethod method, std::unique_ptr<Ichor::v1::RouteMatcher> matcher, std::function<Task<Ichor::v1::HttpResponse>(Ichor::v1::HttpRequest&)> handler) final;
        void removeRoute(Ichor::v1::HttpMethod method, Ichor::v1::RouteIdType id) final;

        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(Ichor::ScopedServiceProxy<Ichor::v1::ILogger*> logger, IService &isvc);
        void removeDependencyInstance(Ichor::ScopedServiceProxy<Ichor::v1::ILogger*> logger, IService &isvc);
        void addDependencyInstance(Ichor::ScopedServiceProxy<IBoostAsioQueue*> q, IService&);
        void removeDependencyInstance(Ichor::ScopedServiceProxy<IBoostAsioQueue*> q, IService&);

        void fail(beast::error_code, char const* what, bool stopSelf);
        void listen(tcp::endpoint endpoint, net::yield_context yield);

        template <typename SocketT>
        void read(tcp::socket socket, net::yield_context yield);

        template <typename SocketT>
        void sendInternal(std::shared_ptr<Detail::Connection<SocketT>> &connection, http::response<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>> &&res);

        friend DependencyRegister;

        std::unique_ptr<tcp::acceptor> _httpAcceptor{};
        unordered_map<uint64_t, std::shared_ptr<Detail::Connection<beast::tcp_stream>>> _httpStreams{};
        unordered_map<uint64_t, std::shared_ptr<Detail::Connection<beast::ssl_stream<beast::tcp_stream>>>> _sslStreams{};
        std::unique_ptr<net::ssl::context> _sslContext{};
        uint64_t _priority{INTERNAL_EVENT_PRIORITY};
        bool _quit{};
        bool _goingToCleanupStream{};
        std::atomic<int64_t> _finishedListenAndRead{};
        bool _tcpNoDelay{};
        bool _useSsl{};
        uint64_t _streamIdCounter{};
        uint64_t _matchersIdCounter{};
        bool _sendServerHeader{true};
        bool _debug{};
        Ichor::ScopedServiceProxy<Ichor::v1::ILogger*> _logger {};
        unordered_map<Ichor::v1::HttpMethod, unordered_map<std::unique_ptr<Ichor::v1::RouteMatcher>, std::function<Task<Ichor::v1::HttpResponse>(Ichor::v1::HttpRequest&)>>> _handlers{};
        AsyncManualResetEvent _startStopEvent{};
        Ichor::ScopedServiceProxy<IBoostAsioQueue*> _queue {};
    };
}
