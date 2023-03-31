#pragma once

#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/services/network/http/IHttpHostService.h>
#include <ichor/services/network/AsioContextService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <ichor/stl/RealtimeMutex.h>
#include <boost/beast.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/circular_buffer.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace Ichor {
    namespace Detail {
        using HostOutboxMessage = http::response<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>>;
        struct Connection {
            Connection(tcp::socket &&_socket) : socket(std::move(_socket)) {}
            beast::tcp_stream socket;
            boost::circular_buffer<HostOutboxMessage> outbox{10};
            RealtimeMutex mutex{};
        };
    }

    class HttpHostService final : public IHttpHostService, public AdvancedService<HttpHostService> {
    public:
        HttpHostService(DependencyRegister &reg, Properties props);
        ~HttpHostService() final = default;

        std::unique_ptr<HttpRouteRegistration> addRoute(HttpMethod method, std::string_view route, std::function<AsyncGenerator<HttpResponse>(HttpRequest&)> handler) final;
        void removeRoute(HttpMethod method, std::string_view route) final;

        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(ILogger &logger, IService &isvc);
        void removeDependencyInstance(ILogger &logger, IService &isvc);
        void addDependencyInstance(IAsioContextService &logger, IService&);
        void removeDependencyInstance(IAsioContextService &logger, IService&);

        void fail(beast::error_code, char const* what, bool stopSelf);
        void listen(tcp::endpoint endpoint, net::yield_context yield);
        void read(tcp::socket socket, net::yield_context yield);
        void sendInternal(std::shared_ptr<Detail::Connection> &connection, http::response<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>> &&res);

        friend DependencyRegister;

        std::unique_ptr<tcp::acceptor> _httpAcceptor{};
        unordered_map<uint64_t, std::shared_ptr<Detail::Connection>> _httpStreams{};
        RealtimeMutex _streamsMutex{};
        std::atomic<uint64_t> _priority{INTERNAL_EVENT_PRIORITY};
        std::atomic<bool> _quit{};
        std::atomic<bool> _goingToCleanupStream{};
        std::atomic<int64_t> _finishedListenAndRead{};
        std::atomic<bool> _tcpNoDelay{};
        uint64_t _streamIdCounter{};
        bool _sendServerHeader{true};
        std::atomic<ILogger*> _logger{};
        IAsioContextService *_asioContextService{};
        unordered_map<HttpMethod, unordered_map<std::string, std::function<AsyncGenerator<HttpResponse>(HttpRequest&)>, string_hash, std::equal_to<>>> _handlers{};
        AsyncManualResetEvent _startStopEvent{};
        IEventQueue *_queue;
    };
}

#endif
