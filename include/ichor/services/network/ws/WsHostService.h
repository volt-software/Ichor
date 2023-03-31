#pragma once

#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/services/network/IHostService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/network/ws/WsConnectionService.h>
#include <ichor/services/network/ws/WsEvents.h>
#include <ichor/services/network/AsioContextService.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <boost/beast.hpp>
#include <boost/asio/spawn.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace Ichor {
    class WsHostService final : public IHostService, public AdvancedService<WsHostService> {
    public:
        WsHostService(DependencyRegister &reg, Properties props);
        ~WsHostService() final = default;

        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(ILogger &logger, IService &isvc);
        void removeDependencyInstance(ILogger &logger, IService &isvc);
        void addDependencyInstance(IAsioContextService &logger, IService&);
        void removeDependencyInstance(IAsioContextService &logger, IService&);

        AsyncGenerator<IchorBehaviour> handleEvent(NewWsConnectionEvent const &evt);

        friend DependencyRegister;
        friend DependencyManager;

        void fail(beast::error_code, char const* what);
        void listen(tcp::endpoint endpoint, net::yield_context yield);

        std::unique_ptr<tcp::acceptor> _wsAcceptor{};
        std::unique_ptr<net::strand<net::io_context::executor_type>> _strand{};
        uint64_t _priority{INTERNAL_EVENT_PRIORITY};
        std::atomic<bool> _quit{};
        std::atomic<bool> _tcpNoDelay{};
        std::atomic<int64_t> _finishedListenAndRead{};
        ILogger *_logger{};
        IAsioContextService *_asioContextService{};
        std::vector<WsConnectionService*> _connections{};
        EventHandlerRegistration _eventRegistration{};
        AsyncManualResetEvent _startStopEvent{};
        IEventQueue *_queue;
    };
}

#endif
