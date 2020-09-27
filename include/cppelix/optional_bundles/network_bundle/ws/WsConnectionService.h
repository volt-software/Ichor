#pragma once

#ifdef USE_BOOST_BEAST

#include <cppelix/optional_bundles/network_bundle/IConnectionService.h>
#include <cppelix/optional_bundles/logging_bundle/Logger.h>
#include <thread>
#include <boost/beast.hpp>
#include <boost/asio/spawn.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace Cppelix {
    class WsConnectionService final : public IConnectionService, public Service {
    public:
        WsConnectionService(DependencyRegister &reg, CppelixProperties props);
        ~WsConnectionService() final = default;

        bool start() final;
        bool stop() final;

        void addDependencyInstance(ILogger *logger);
        void removeDependencyInstance(ILogger *logger);

        void send(std::vector<uint8_t>&& msg) final;
        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        void fail(beast::error_code, char const* what);
        void connect(net::yield_context yield);
        void read(net::yield_context &yield);

        std::unique_ptr<net::io_context> _wsContext{};
        std::unique_ptr<websocket::stream<beast::tcp_stream>> _ws{};
        int _attempts{};
        std::atomic<uint64_t> _priority{};
        std::atomic<bool> _connected{};
        std::atomic<bool> _connecting{};
        std::atomic<bool> _quit{};
        std::thread _connectThread{};
        ILogger *_logger{nullptr};
    };
}

#endif