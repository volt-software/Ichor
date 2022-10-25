#pragma once

#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/services/network/http/IHttpService.h>
#include <ichor/services/network/http/HttpContextService.h>
#include <ichor/services/logging/Logger.h>
#include <boost/beast.hpp>
#include <boost/asio/spawn.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace Ichor {
    class HttpHostService final : public IHttpService, public Service<HttpHostService> {
    public:
        HttpHostService(DependencyRegister &reg, Properties props, DependencyManager *mng);
        ~HttpHostService() final = default;

        StartBehaviour start() final;
        StartBehaviour stop() final;

        void addDependencyInstance(ILogger *logger, IService *isvc);
        void removeDependencyInstance(ILogger *logger, IService *isvc);
        void addDependencyInstance(IHttpContextService *logger, IService *);
        void removeDependencyInstance(IHttpContextService *logger, IService *);

        std::unique_ptr<HttpRouteRegistration> addRoute(HttpMethod method, std::string_view route, std::function<HttpResponse(HttpRequest&)> handler) final;
        void removeRoute(HttpMethod method, std::string_view route) final;

        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        void fail(beast::error_code, char const* what);
        void listen(tcp::endpoint endpoint, net::yield_context yield);
        void read(tcp::socket socket, net::yield_context yield);

        std::unique_ptr<tcp::acceptor> _httpAcceptor{};
        std::unique_ptr<beast::tcp_stream> _httpStream{};
        std::atomic<uint64_t> _priority{INTERNAL_EVENT_PRIORITY};
        std::atomic<bool> _quit{};
        std::atomic<bool> _goingToCleanupStream{};
        std::atomic<bool> _cleanedupStream{};
        std::atomic<bool> _tcpNoDelay{};
        ILogger *_logger{nullptr};
        IHttpContextService *_httpContextService{nullptr};
        std::unordered_map<HttpMethod, std::unordered_map<std::string, std::function<HttpResponse(HttpRequest&)>, string_hash, std::equal_to<>>> _handlers{};
    };
}

#endif