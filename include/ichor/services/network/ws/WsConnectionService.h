#pragma once

#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/services/network/IConnectionService.h>
#include <ichor/services/network/IHostService.h>
#include <ichor/services/network/AsioContextService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <ichor/stl/RealtimeMutex.h>
#include <queue>
#include <boost/beast.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/circular_buffer.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace Ichor {
    namespace Detail {
        struct WsConnectionOutboxMessage {
            uint64_t msgId;
            std::vector<uint8_t> msg;
        };
    }

    class WsConnectionService final : public IConnectionService, public AdvancedService<WsConnectionService> {
    public:
        WsConnectionService(DependencyRegister &reg, Properties props, DependencyManager *mng);
        ~WsConnectionService() final = default;

        tl::expected<uint64_t, SendErrorReason> sendAsync(std::vector<uint8_t>&& msg) final;
        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        AsyncGenerator<tl::expected<void, StartError>> start() final;
        AsyncGenerator<void> stop() final;

        void addDependencyInstance(ILogger *logger, IService *isvc);
        void removeDependencyInstance(ILogger *logger, IService *isvc);

        void addDependencyInstance(IHostService *, IService *isvc);
        void removeDependencyInstance(IHostService *, IService *isvc);

        void addDependencyInstance(IAsioContextService *logger, IService *);
        void removeDependencyInstance(IAsioContextService *logger, IService *);

        friend DependencyRegister;
        friend DependencyManager;

        void fail(beast::error_code, char const* what);
        void accept(net::yield_context yield); // for when a new connection from WsHost is established
        void connect(net::yield_context yield); // for when connecting as a client
        void read(net::yield_context &yield);

        std::shared_ptr<websocket::stream<beast::tcp_stream>> _ws{};
        uint64_t _msgIdCounter{};
        std::atomic<uint64_t> _priority{};
        std::atomic<bool> _connected{};
        std::atomic<bool> _quit{};
        ILogger *_logger{nullptr};
        IAsioContextService *_asioContextService{nullptr};
        std::unique_ptr<net::strand<net::io_context::executor_type>> _strand{};
        std::atomic<int64_t> _finishedListenAndRead{};
        AsyncManualResetEvent _startStopEvent{};
        boost::circular_buffer<Detail::WsConnectionOutboxMessage> _outbox{10};
    };
}

#endif
