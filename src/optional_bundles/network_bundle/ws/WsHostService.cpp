#ifdef USE_BOOST_BEAST

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/network_bundle/IConnectionService.h>
#include <ichor/optional_bundles/network_bundle/ws/WsHostService.h>
#include <ichor/optional_bundles/network_bundle/ws/WsConnectionService.h>
#include <ichor/optional_bundles/network_bundle/ws/WsEvents.h>
#include <ichor/optional_bundles/network_bundle/NetworkDataEvent.h>

template<class NextLayer>
void setup_stream(websocket::stream<NextLayer>& ws)
{
    // These values are tuned for Autobahn|Testsuite, and
    // should also be generally helpful for increased performance.

    websocket::permessage_deflate pmd;
    pmd.client_enable = true;
    pmd.server_enable = true;
    pmd.compLevel = 3;
    ws.set_option(pmd);

    ws.auto_fragment(false);
}


Ichor::WsHostService::WsHostService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
    reg.registerDependency<ILogger>(this, true);
}

bool Ichor::WsHostService::start() {
    if(getProperties()->contains("Priority")) {
        _priority = Ichor::any_cast<uint64_t>(getProperties()->operator[]("Priority"));
    }

    if(!getProperties()->contains("Port") || !getProperties()->contains("Address")) {
        getManager()->pushPrioritisedEvent<UnrecoverableErrorEvent>(getServiceId(), _priority, 0, "Missing port or address when starting WsHostService");
        return false;
    }

    _eventRegistration = getManager()->registerEventHandler<NewWsConnectionEvent>(this, getServiceId());

    _wsContext = std::make_unique<net::io_context>(1);
    auto const& address = net::ip::make_address(Ichor::any_cast<std::string&>(getProperties()->operator[]("Address")));
    auto port = Ichor::any_cast<uint16_t>(getProperties()->operator[]("Port"));

    net::spawn(*_wsContext, [this, address = std::move(address), port](net::yield_context yield){
        try {
            listen(tcp::endpoint{address, port}, std::move(yield));
        } catch (std::runtime_error &e) {
            ICHOR_LOG_ERROR(_logger, "caught std runtime_error {}", e.what());
            getManager()->pushEvent<StopServiceEvent>(getServiceId(), getServiceId());
        } catch (...) {
            ICHOR_LOG_ERROR(_logger, "caught unknown error");
            getManager()->pushEvent<StopServiceEvent>(getServiceId(), getServiceId());
        }
    });

    _listenThread = std::thread([this]{
        try {
            _wsContext->run();
        } catch (std::runtime_error &e) {
            ICHOR_LOG_ERROR(_logger, "caught std runtime_error {}", e.what());
            getManager()->pushEvent<StopServiceEvent>(getServiceId(), getServiceId());
        } catch (...) {
            ICHOR_LOG_ERROR(_logger, "caught unknown error");
            getManager()->pushEvent<StopServiceEvent>(getServiceId(), getServiceId());
        }
    });

#ifdef __linux__
    pthread_setname_np(_listenThread.native_handle(), fmt::format("WsHost #{}", getServiceId()).c_str());
#endif

    return true;
}

bool Ichor::WsHostService::stop() {
    ICHOR_LOG_TRACE(_logger, "trying to stop WsHostService {}", getServiceId());
    _quit = true;

    for(auto conn : _connections) {
        conn->stop();
    }
    ICHOR_LOG_TRACE(_logger, "connections closed WsHostService {}", getServiceId());

    _wsAcceptor->close();


    ICHOR_LOG_TRACE(_logger, "joining WsHostService {}", getServiceId());

    _listenThread.join();

    ICHOR_LOG_TRACE(_logger, "wsContext->stop() WsHostService {}", getServiceId());

    _wsContext->stop();
    _wsContext = nullptr;

    return true;
}

void Ichor::WsHostService::addDependencyInstance(ILogger *logger, IService *) {
    _logger = logger;
    ICHOR_LOG_TRACE(_logger, "Inserted logger");
}

void Ichor::WsHostService::removeDependencyInstance(ILogger *logger, IService *) {
    _logger = nullptr;
}

Ichor::Generator<bool> Ichor::WsHostService::handleEvent(Ichor::NewWsConnectionEvent const * const evt) {
    auto connection = getManager()->createServiceManager<WsConnectionService, IConnectionService>(Ichor::make_properties(getMemoryResource(),
        IchorProperty{"WsHostServiceId", Ichor::make_any<uint64_t>(getMemoryResource(), getServiceId())},
        IchorProperty{"Socket", Ichor::make_any<decltype(evt->_socket)>(getMemoryResource(), std::move(evt->_socket))},
        IchorProperty{"Executor", Ichor::make_any<decltype(evt->_executor)>(getMemoryResource(), evt->_executor)}
        ));
    _connections.push_back(connection);

    co_return (bool)PreventOthersHandling;
}

void Ichor::WsHostService::setPriority(uint64_t priority) {
    _priority.store(priority, std::memory_order_release);
}

uint64_t Ichor::WsHostService::getPriority() {
    return _priority.load(std::memory_order_acquire);
}

void Ichor::WsHostService::fail(beast::error_code ec, const char *what) {
    ICHOR_LOG_ERROR(_logger, "Boost.BEAST fail: {}, {}", what, ec.message());
    getManager()->pushPrioritisedEvent<StopServiceEvent>(getServiceId(), _priority, getServiceId());
}

void Ichor::WsHostService::listen(tcp::endpoint endpoint, net::yield_context yield)
{
    beast::error_code ec;

    _wsAcceptor = std::make_unique<tcp::acceptor>(*_wsContext);
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

    while(!_quit.load(std::memory_order_acquire))
    {
        tcp::socket socket(*_wsContext);

        // tcp accept new connections
        _wsAcceptor->async_accept(socket, yield[ec]);
        if(ec)
        {
            fail(ec, "accept");
            continue;
        }

        getManager()->pushPrioritisedEvent<NewWsConnectionEvent>(getServiceId(), _priority.load(std::memory_order_acquire), CopyIsMoveWorkaround(std::move(socket)), _wsAcceptor->get_executor());
    }
}

#endif