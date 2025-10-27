#include <ichor/DependencyManager.h>
#include <ichor/services/network/IConnectionService.h>
#include <ichor/services/network/boost/WsHostService.h>
#include <ichor/services/network/boost/WsConnectionService.h>
#include <ichor/services/network/ws/WsEvents.h>
#include <ichor/services/network/http/HttpScopeGuards.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/ServiceExecutionScope.h>

Ichor::Boost::v1::WsHostService::WsHostService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
    reg.registerDependency<Ichor::v1::ILogger>(this, DependencyFlags::REQUIRED);
    reg.registerDependency<IBoostAsioQueue>(this, DependencyFlags::REQUIRED);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::Boost::v1::WsHostService::start() {
    auto addrIt = getProperties().find("Address");
    auto portIt = getProperties().find("Port");

    if(addrIt == getProperties().end()) {
        ICHOR_LOG_ERROR(_logger, "Missing address");
        co_return tl::unexpected(StartError::FAILED);
    }
    if(portIt == getProperties().end()) {
        ICHOR_LOG_ERROR(_logger, "Missing port");
        co_return tl::unexpected(StartError::FAILED);
    }

    if(auto propIt = getProperties().find("Priority"); propIt != getProperties().end()) {
        _priority = Ichor::v1::any_cast<uint64_t>(propIt->second);
    }
    if(auto propIt = getProperties().find("NoDelay"); propIt != getProperties().end()) {
        _tcpNoDelay = Ichor::v1::any_cast<bool>(propIt->second);
    }

    _eventRegistration = GetThreadLocalManager().registerEventHandler<Ichor::v1::NewWsConnectionEvent>(this, this, getServiceId());

    auto address = net::ip::make_address(Ichor::v1::any_cast<std::string&>(addrIt->second));
    auto port = Ichor::v1::any_cast<uint16_t>(portIt->second);
    _strand = std::make_unique<net::strand<net::io_context::executor_type>>(_queue->getContext().get_executor());

    net::spawn(*_strand, [this, address = std::move(address), port](net::yield_context yield) {
        Ichor::v1::ScopeGuardAtomicCount const guard{_finishedListenAndRead};
#if ICHOR_EXCEPTIONS_ENABLED
        try {
#endif
            listen(tcp::endpoint{address, port}, std::move(yield));
#if ICHOR_EXCEPTIONS_ENABLED
        } catch (std::runtime_error &e) {
            ICHOR_LOG_ERROR(_logger, "caught std runtime_error {}", e.what());
            _queue->pushEvent<StopServiceEvent>(getServiceId(), getServiceId());
        } catch (...) {
            ICHOR_LOG_ERROR(_logger, "caught unknown error");
            _queue->pushEvent<StopServiceEvent>(getServiceId(), getServiceId());
        }
#endif
    }ASIO_SPAWN_COMPLETION_TOKEN);

    co_return {};
}

Ichor::Task<void> Ichor::Boost::v1::WsHostService::stop() {
    INTERNAL_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! trying to stop WsHostService {}", getServiceId());
    _quit = true;

    for(auto conn : _connections) {
        _queue->pushEvent<StopServiceEvent>(getServiceId(), conn, true);
    }

    _wsAcceptor->close();

    INTERNAL_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! pre-await {} {}", getServiceId(), _startStopEvent.is_set());
    co_await _startStopEvent;

    while(_finishedListenAndRead.load(std::memory_order_acquire) != 0) {
        _startStopEvent.reset();
        _queue->pushEvent<RunFunctionEvent>(getServiceId(), [this]() {
            _startStopEvent.set();
        });
        co_await _startStopEvent;
    }

    INTERNAL_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! stopped WsHostService {}", getServiceId());
    _strand = nullptr;
    _wsAcceptor = nullptr;

    co_return;
}

void Ichor::Boost::v1::WsHostService::addDependencyInstance(Ichor::ScopedServiceProxy<Ichor::v1::ILogger*> logger, IService &) {
    _logger = std::move(logger);
}

void Ichor::Boost::v1::WsHostService::removeDependencyInstance(Ichor::ScopedServiceProxy<Ichor::v1::ILogger*> logger, IService&) {
    _logger = nullptr;
}

void Ichor::Boost::v1::WsHostService::addDependencyInstance(Ichor::ScopedServiceProxy<IBoostAsioQueue*> q, IService&) {
    _queue = std::move(q);
}

void Ichor::Boost::v1::WsHostService::removeDependencyInstance(Ichor::ScopedServiceProxy<IBoostAsioQueue*>, IService&) {
    _queue = nullptr;
}

Ichor::AsyncGenerator<Ichor::IchorBehaviour> Ichor::Boost::v1::WsHostService::handleEvent(Ichor::v1::NewWsConnectionEvent const &evt) {
    if(_quit) {
        co_return {};
    }

    auto connection = GetThreadLocalManager().createServiceManager<WsConnectionService<Ichor::v1::IHostConnectionService>, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService>(Properties{
        {"WsHostServiceId", Ichor::v1::make_any<ServiceIdType>(getServiceId())},
        {"Socket", Ichor::v1::make_unformattable_any<decltype(evt._socket)>(evt._socket)}
    });
    _connections.push_back(connection->getServiceId());

    co_return {};
}

void Ichor::Boost::v1::WsHostService::setPriority(uint64_t priority) {
    _priority = priority;
}

uint64_t Ichor::Boost::v1::WsHostService::getPriority() {
    return _priority;
}

void Ichor::Boost::v1::WsHostService::fail(beast::error_code ec, const char *what) {
    ICHOR_LOG_ERROR(_logger, "Boost.BEAST fail: {}, {}", what, ec.message());
    _queue->pushPrioritisedEvent<StopServiceEvent>(getServiceId(), _priority, getServiceId());
    INTERNAL_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! _startStopEvent set {}", getServiceId());
    _startStopEvent.set();
}

void Ichor::Boost::v1::WsHostService::listen(tcp::endpoint endpoint, net::yield_context yield)
{
    beast::error_code ec;

    _wsAcceptor = std::make_unique<tcp::acceptor>(_queue->getContext());
    _wsAcceptor->open(endpoint.protocol(), ec);
    if(ec) {
        return fail(ec, "open");
    }

    _wsAcceptor->set_option(net::socket_base::reuse_address(true), ec);
    if(ec) {
        return fail(ec, "set_option");
    }

    _wsAcceptor->bind(endpoint, ec);
    if(ec) {
        return fail(ec, "bind");
    }

    _wsAcceptor->listen(net::socket_base::max_listen_connections, ec);
    if(ec) {
        return fail(ec, "listen");
    }

    while(!_quit && !_queue->fibersShouldStop()) {
        tcp::socket socket(_queue->getContext());

        // tcp accept new connections
        _wsAcceptor->async_accept(socket, yield[ec]);
        if(ec) {
            return fail(ec, "accept");
        }
        if(_quit) {
            break;
        }

        socket.set_option(tcp::no_delay(_tcpNoDelay));

        _queue->pushPrioritisedEvent<Ichor::v1::NewWsConnectionEvent>(getServiceId(), _priority, std::make_shared<websocket::stream<beast::tcp_stream>>(std::move(socket)));
    }

    INTERNAL_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! push2 {}", getServiceId());
    _startStopEvent.set();
}
