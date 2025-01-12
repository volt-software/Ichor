#pragma once

#include <ichor/event_queues/BoostAsioQueue.h>
#include <ichor/services/network/http/IHttpConnectionService.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <ichor/coroutines/AsyncReturningManualResetEvent.h>
#include <ichor/services/logging/Logger.h>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/circular_buffer.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace Ichor::Boost {
    namespace Detail {
        struct ConnectionOutboxMessage {
            Ichor::HttpMethod method;
            std::string_view route;
            AsyncReturningManualResetEvent<tl::expected<Ichor::HttpResponse, Ichor::HttpError>>* event;
            unordered_map<std::string, std::string>* headers;
            std::vector<uint8_t>* body;
        };
    }

    /**
     * Service for connecting to an HTTP/1.1 server using boost. Requires an IBoostAsioQueue and a logger.
     *
     * Properties:
     * - "Address" std::string - What address to connect to (required)
     * - "Port" uint16_t - What port to connect to (required)
     * - "Priority" uint64_t - What priority to insert events with (e.g. when getting a response from the server)
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

        Task<tl::expected<HttpResponse, HttpError>> sendAsync(HttpMethod method, std::string_view route, unordered_map<std::string, std::string> &&headers, std::vector<uint8_t>&& msg) final;

        Task<void> close() final;

        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(ILogger &logger, IService &);
        void removeDependencyInstance(ILogger &logger, IService&);
        void addDependencyInstance(IBoostAsioQueue &q, IService&);
        void removeDependencyInstance(IBoostAsioQueue &q, IService&);

        void fail(beast::error_code, char const* what);
        void connect(tcp::endpoint endpoint, net::yield_context yield);

        friend DependencyRegister;

        std::unique_ptr<beast::tcp_stream> _httpStream{};
        std::unique_ptr<beast::ssl_stream<beast::tcp_stream>> _sslStream{};
        std::unique_ptr<net::ssl::context> _sslContext{};
        uint64_t _priority{INTERNAL_EVENT_PRIORITY};
        bool _quit{};
        bool _connecting{};
        bool _connected{};
        bool _tcpNoDelay{};
        bool _useSsl{};
        std::atomic<int64_t> _finishedListenAndRead{};
        ILogger* _logger{};
        boost::circular_buffer<Detail::ConnectionOutboxMessage> _outbox{10};
        AsyncManualResetEvent _startStopEvent{};
        IBoostAsioQueue *_queue;
        bool _debug{};
        uint64_t _tryConnectIntervalMs{100};
        uint64_t _timeoutMs{10'000};
        uint64_t _timeWhenDisconnected{};
    };
}
