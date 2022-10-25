#pragma once

#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/stl/RealtimeMutex.h>
#include <ichor/services/network/IConnectionService.h>
#include <ichor/services/network/IHostService.h>
#include <ichor/services/network/http/HttpContextService.h>
#include <ichor/services/logging/Logger.h>
#include <queue>
#include <boost/beast.hpp>
#include <boost/asio/spawn.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

#include <iostream>

namespace Ichor {
    class WsConnectionService final : public IConnectionService, public Service<WsConnectionService> {
    public:
        WsConnectionService(DependencyRegister &reg, Properties props, DependencyManager *mng);
        ~WsConnectionService() final = default;

        StartBehaviour start() final;
        StartBehaviour stop() final;

        void addDependencyInstance(ILogger *logger, IService *isvc);
        void removeDependencyInstance(ILogger *logger, IService *isvc);

        void addDependencyInstance(IHostService *, IService *isvc);
        void removeDependencyInstance(IHostService *, IService *isvc);

        void addDependencyInstance(IHttpContextService *logger, IService *);
        void removeDependencyInstance(IHttpContextService *logger, IService *);

        uint64_t sendAsync(std::vector<uint8_t>&& msg) final;
        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        void fail(beast::error_code, char const* what);
        void accept(net::yield_context yield); // for when a new connection from WsHost is established
        void connect(net::yield_context yield); // for when connecting as a client
        void read(net::yield_context &yield);

        std::unique_ptr<websocket::stream<beast::tcp_stream>> _ws{};
        uint64_t _msgIdCounter{};
        std::atomic<uint64_t> _priority{};
        std::atomic<bool> _connected{};
        std::atomic<bool> _connecting{};
        std::atomic<bool> _quit{};
        ILogger *_logger{nullptr};
        IHttpContextService *_httpContextService{nullptr};
        RealtimeMutex _mutex{};
    };
}

#endif