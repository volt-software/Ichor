#pragma once

#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/services/network/IHostService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/network/ws/WsConnectionService.h>
#include <ichor/services/network/ws/WsEvents.h>
#include <ichor/services/network/http/HttpContextService.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <boost/beast.hpp>
#include <boost/asio/spawn.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace Ichor {
    class WsHostService final : public IHostService, public Service<WsHostService> {
    public:
        WsHostService(DependencyRegister &reg, Properties props, DependencyManager *mng);
        ~WsHostService() final = default;

        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        AsyncGenerator<void> start() final;
        AsyncGenerator<void> stop() final;

        void addDependencyInstance(ILogger *logger, IService *isvc);
        void removeDependencyInstance(ILogger *logger, IService *isvc);
        void addDependencyInstance(IHttpContextService *logger, IService *);
        void removeDependencyInstance(IHttpContextService *logger, IService *);

        AsyncGenerator<IchorBehaviour> handleEvent(NewWsConnectionEvent const &evt);

        friend DependencyRegister;
        friend DependencyManager;

        void fail(beast::error_code, char const* what);
        void listen(tcp::endpoint endpoint, net::yield_context yield);

        std::unique_ptr<tcp::acceptor> _wsAcceptor{};
        uint64_t _priority{INTERNAL_EVENT_PRIORITY};
        std::atomic<bool> _quit{};
        std::atomic<bool> _tcpNoDelay{};
        ILogger *_logger{nullptr};
        IHttpContextService *_httpContextService{nullptr};
        std::vector<WsConnectionService*> _connections{};
        EventHandlerRegistration _eventRegistration{};
        AsyncManualResetEvent _startStopEvent{};
    };
}

#endif