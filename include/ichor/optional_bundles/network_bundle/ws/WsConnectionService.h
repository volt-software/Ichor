#pragma once

#ifdef USE_BOOST_BEAST

#include <ichor/optional_bundles/network_bundle/IConnectionService.h>
#include <ichor/optional_bundles/network_bundle/IHostService.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <thread>
#include <queue>
#include <boost/beast.hpp>
#include <boost/asio/spawn.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace Ichor {
    class WsConnectionService final : public IConnectionService, public Service<WsConnectionService> {
    public:
        WsConnectionService(DependencyRegister &reg, IchorProperties props, DependencyManager *mng);
        ~WsConnectionService() final = default;

        bool start() final;
        bool stop() final;

        void addDependencyInstance(ILogger *logger);
        void removeDependencyInstance(ILogger *logger);

        void addDependencyInstance(IHostService *);
        void removeDependencyInstance(IHostService *);

        /**
         * Asynchronous send, if send queue is full, doesn't send this message and returns false
         * @param msg message to send
         * @return true if added to buffer, false if full
         */
        bool send(std::vector<uint8_t>&& msg) final;
        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        void fail(beast::error_code, char const* what);
        void sendStrand(net::yield_context yield);
        void accept(net::yield_context yield); // for when a new connection from WsHost is established
        void connect(net::yield_context yield); // for when connecting as a client
        void read(net::yield_context &yield);
        void cancelSendTimer();

        std::unique_ptr<net::io_context> _wsContext{};
        std::unique_ptr<websocket::stream<beast::tcp_stream>> _ws{};
        std::unique_ptr<net::steady_timer> _sendTimer; // used as condition variable
        std::queue<std::vector<uint8_t>> _msgQueue{};
        int _attempts{};
        std::atomic<uint64_t> _priority{};
        std::atomic<bool> _connected{};
        std::atomic<bool> _connecting{};
        std::atomic<bool> _quit{};
        std::atomic<bool> _sendStrandDone{};
        RealtimeMutex _queueMutex{};
        std::thread _connectThread{};
        ILogger *_logger{nullptr};
    };
}

#endif