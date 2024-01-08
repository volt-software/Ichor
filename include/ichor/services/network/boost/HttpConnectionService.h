#pragma once

#include <ichor/services/network/http/IHttpConnectionService.h>
#include <ichor/services/network/boost/AsioContextService.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/stl/RealtimeMutex.h>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/circular_buffer.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace Ichor {
    namespace Detail {
        struct ConnectionOutboxMessage {
            Ichor::HttpMethod method;
            std::string_view route;
            AsyncManualResetEvent* event;
            HttpResponse* response;
            unordered_map<std::string, std::string>* headers;
            std::vector<uint8_t>* body;
        };
    }

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

        void addDependencyInstance(ILogger &logger, IService &);
        void removeDependencyInstance(ILogger &logger, IService&);
        void addDependencyInstance(IAsioContextService &logger, IService&);
        void removeDependencyInstance(IAsioContextService &logger, IService&);

        void fail(beast::error_code, char const* what);
        void connect(tcp::endpoint endpoint, net::yield_context yield);

        friend DependencyRegister;

        std::unique_ptr<beast::tcp_stream> _httpStream{};
        std::unique_ptr<beast::ssl_stream<beast::tcp_stream>> _sslStream{};
        std::unique_ptr<net::ssl::context> _sslContext{};
        std::atomic<uint64_t> _priority{INTERNAL_EVENT_PRIORITY};
        std::atomic<bool> _quit{};
        std::atomic<bool> _connecting{};
        std::atomic<bool> _connected{};
        std::atomic<bool> _tcpNoDelay{};
        std::atomic<bool> _useSsl{};
        std::atomic<int64_t> _finishedListenAndRead{};
        std::atomic<ILogger*> _logger{};
        IAsioContextService *_asioContextService{};
        boost::circular_buffer<Detail::ConnectionOutboxMessage> _outbox{10};
        RealtimeMutex _outboxMutex{};
        AsyncManualResetEvent _startStopEvent{};
        IEventQueue *_queue;
    };
}