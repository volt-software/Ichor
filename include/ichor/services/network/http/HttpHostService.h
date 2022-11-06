#pragma once

#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/services/network/http/IHttpService.h>
#include <ichor/services/network/http/HttpContextService.h>
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
        struct HostOutboxMessage {
            uint64_t streamId{};
            http::response<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>> res{};
        };
    }

    class HttpHostService final : public IHttpService, public Service<HttpHostService> {
    public:
        HttpHostService(DependencyRegister &reg, Properties props, DependencyManager *mng);
        ~HttpHostService() final = default;

        std::unique_ptr<HttpRouteRegistration> addRoute(HttpMethod method, std::string_view route, std::function<AsyncGenerator<HttpResponse>(HttpRequest&)> handler) final;
        void removeRoute(HttpMethod method, std::string_view route) final;

        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        StartBehaviour start() final;
        StartBehaviour stop() final;

        void addDependencyInstance(ILogger *logger, IService *isvc);
        void removeDependencyInstance(ILogger *logger, IService *isvc);
        void addDependencyInstance(IHttpContextService *logger, IService *);
        void removeDependencyInstance(IHttpContextService *logger, IService *);

        void fail(beast::error_code, char const* what, bool stopSelf);
        void listen(tcp::endpoint endpoint, net::yield_context yield);
        void read(tcp::socket socket, net::yield_context yield);
        void sendInternal(uint64_t streamId, http::response<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>> &&res);

        friend DependencyRegister;

        std::unique_ptr<tcp::acceptor> _httpAcceptor{};
        // _httpStreams should only be modified from the boost thread
        unordered_map<uint64_t, std::unique_ptr<beast::tcp_stream>> _httpStreams{};
        std::atomic<uint64_t> _priority{INTERNAL_EVENT_PRIORITY};
        std::atomic<bool> _quit{};
        std::atomic<bool> _goingToCleanupStream{};
        std::atomic<bool> _cleanedupStream{};
        std::atomic<int64_t> _finishedListenAndRead{};
        std::atomic<bool> _tcpNoDelay{};
        uint64_t _streamIdCounter{};
        ILogger *_logger{nullptr};
        IHttpContextService *_httpContextService{nullptr};
        unordered_map<HttpMethod, unordered_map<std::string, std::function<AsyncGenerator<HttpResponse>(HttpRequest&)>, string_hash, std::equal_to<>>> _handlers{};
        boost::circular_buffer<Detail::HostOutboxMessage> _outbox{10};
    };
}

#endif