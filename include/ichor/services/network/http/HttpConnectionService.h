#pragma once

#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/services/network/http/IHttpConnectionService.h>
#include <ichor/services/network/http/HttpContextService.h>
#include <ichor/services/logging/Logger.h>
#include <boost/beast.hpp>
#include <boost/asio/spawn.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace Ichor {
    class HttpConnectionService final : public IHttpConnectionService, public Service<HttpConnectionService> {
    public:
        HttpConnectionService(DependencyRegister &reg, Properties props, DependencyManager *mng);
        ~HttpConnectionService() final = default;

        StartBehaviour start() final;
        StartBehaviour stop() final;

        void addDependencyInstance(ILogger *logger, IService *);
        void removeDependencyInstance(ILogger *logger, IService *);
        void addDependencyInstance(IHttpContextService *logger, IService *);
        void removeDependencyInstance(IHttpContextService *logger, IService *);

        AsyncGenerator<HttpResponse> sendAsync(HttpMethod method, std::string_view route, std::vector<HttpHeader> &&headers, std::vector<uint8_t>&& msg) final;

        bool close() final;

        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        void fail(beast::error_code, char const* what);
        void connect(tcp::endpoint endpoint, net::yield_context yield);

        std::unique_ptr<beast::tcp_stream> _httpStream{};
        std::atomic<uint64_t> _priority{INTERNAL_EVENT_PRIORITY};
        std::atomic<bool> _quit{};
        std::atomic<bool> _connecting{};
        std::atomic<bool> _connected{};
        std::atomic<bool> _stopping{};
        std::atomic<bool> _tcpNoDelay{};
        int _attempts{};
//        uint64_t _msgId{};
        ILogger *_logger{nullptr};
        IHttpContextService *_httpContextService{nullptr};
    };
}

#endif