#pragma once

#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/services/network/http/IHttpConnectionService.h>
#include <ichor/services/network/http/HttpContextService.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <ichor/services/logging/Logger.h>
#include <boost/beast.hpp>
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
            std::vector<HttpHeader>* headers;
            std::vector<uint8_t>* body;
        };
    }

    class HttpConnectionService final : public IHttpConnectionService, public Service<HttpConnectionService> {
    public:
        HttpConnectionService(DependencyRegister &reg, Properties props, DependencyManager *mng);
        ~HttpConnectionService() final = default;

        AsyncGenerator<HttpResponse> sendAsync(HttpMethod method, std::string_view route, std::vector<HttpHeader> &&headers, std::vector<uint8_t>&& msg) final;

        bool close() final;

        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        StartBehaviour start() final;
        StartBehaviour stop() final;

        void addDependencyInstance(ILogger *logger, IService *);
        void removeDependencyInstance(ILogger *logger, IService *);
        void addDependencyInstance(IHttpContextService *logger, IService *);
        void removeDependencyInstance(IHttpContextService *logger, IService *);

        void fail(beast::error_code, char const* what);
        void connect(tcp::endpoint endpoint, net::yield_context yield);

        friend DependencyRegister;

        std::unique_ptr<beast::tcp_stream> _httpStream{};
        std::atomic<uint64_t> _priority{INTERNAL_EVENT_PRIORITY};
        std::atomic<bool> _quit{};
        std::atomic<bool> _connecting{};
        std::atomic<bool> _stopping{};
        std::atomic<bool> _connected{};
        std::atomic<bool> _tcpNoDelay{};
        std::atomic<int64_t> _finishedListenAndRead{};
        int _attempts{};
        std::atomic<ILogger*> _logger{nullptr};
        IHttpContextService *_httpContextService{nullptr};
        boost::circular_buffer<Detail::ConnectionOutboxMessage> _outbox{10};
    };
}

#endif