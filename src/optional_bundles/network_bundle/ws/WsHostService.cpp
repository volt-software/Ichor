#ifdef USE_BOOST_BEAST

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/network_bundle/IConnectionService.h>
#include <ichor/optional_bundles/network_bundle/ws/WsHostService.h>
#include <ichor/optional_bundles/network_bundle/ws/WsConnectionService.h>
#include <ichor/optional_bundles/network_bundle/ws/WsEvents.h>
#include <ichor/optional_bundles/network_bundle/NetworkEvents.h>

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
    reg.registerDependency<IHttpContextService>(this, true);
}

Ichor::StartBehaviour Ichor::WsHostService::start() {
    if(getProperties().contains("Priority")) {
        _priority = Ichor::any_cast<uint64_t>(getProperties().operator[]("Priority"));
    }

    if(!getProperties().contains("Port") || !getProperties().contains("Address")) {
        getManager()->pushPrioritisedEvent<UnrecoverableErrorEvent>(getServiceId(), _priority, 0, "Missing port or address when starting WsHostService");
        return Ichor::StartBehaviour::FAILED_DO_NOT_RETRY;
    }

    _eventRegistration = getManager()->registerEventHandler<NewWsConnectionEvent>(this, getServiceId());

    auto address = net::ip::make_address(Ichor::any_cast<std::string&>(getProperties().operator[]("Address")));
    auto port = Ichor::any_cast<uint16_t>(getProperties().operator[]("Port"));

    net::spawn(*_httpContextService->getContext(), [this, address = std::move(address), port](net::yield_context yield){
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

    return Ichor::StartBehaviour::SUCCEEDED;
}

Ichor::StartBehaviour Ichor::WsHostService::stop() {
    INTERNAL_DEBUG("trying to stop WsHostService {}", getServiceId());
//    ICHOR_LOG_TRACE(_logger, "trying to stop WsHostService {}", getServiceId());
    _quit = true;

    bool canStop = true;
    for(auto conn : _connections) {
        canStop &= conn->stop() == StartBehaviour::SUCCEEDED;
    }

    if(canStop) {
        INTERNAL_DEBUG("all connections closed WsHostService {}", getServiceId());
//        ICHOR_LOG_TRACE(_logger, "all connections closed WsHostService {}", getServiceId());
        _wsAcceptor->close();
    }

    return canStop ? Ichor::StartBehaviour::SUCCEEDED : Ichor::StartBehaviour::FAILED_AND_RETRY;
}

void Ichor::WsHostService::addDependencyInstance(ILogger *logger, IService *) {
    _logger = logger;
}

void Ichor::WsHostService::removeDependencyInstance(ILogger *logger, IService *) {
    _logger = nullptr;
}

void Ichor::WsHostService::addDependencyInstance(IHttpContextService *httpContextService, IService *) {
    _httpContextService = httpContextService;
}

void Ichor::WsHostService::removeDependencyInstance(IHttpContextService *logger, IService *) {
    stop();
    _httpContextService = nullptr;
}

Ichor::Generator<bool> Ichor::WsHostService::handleEvent(Ichor::NewWsConnectionEvent const * const evt) {
    auto connection = getManager()->createServiceManager<WsConnectionService, IConnectionService>(Ichor::make_properties(getMemoryResource(),
        IchorProperty{"WsHostServiceId", Ichor::make_any<uint64_t>(getMemoryResource(), getServiceId())},
        IchorProperty{"Socket", Ichor::make_any<decltype(evt->_socket)>(getMemoryResource(), std::move(evt->_socket))}
        ));
    _connections.push_back(connection);

    co_return (bool)PreventOthersHandling;
}

void Ichor::WsHostService::setPriority(uint64_t priority) {
    _priority = priority;
}

uint64_t Ichor::WsHostService::getPriority() {
    return _priority;
}

void Ichor::WsHostService::fail(beast::error_code ec, const char *what) {
    ICHOR_LOG_ERROR(_logger, "Boost.BEAST fail: {}, {}", what, ec.message());
    getManager()->pushPrioritisedEvent<StopServiceEvent>(getServiceId(), _priority, getServiceId());
}

void Ichor::WsHostService::listen(tcp::endpoint endpoint, net::yield_context yield)
{
    beast::error_code ec;

    _wsAcceptor = Ichor::make_unique<tcp::acceptor>(getMemoryResource(), *_httpContextService->getContext());
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

    while(!_quit && !_httpContextService->fibersShouldStop())
    {
        tcp::socket socket(*_httpContextService->getContext());

        // tcp accept new connections
        _wsAcceptor->async_accept(socket, yield[ec]);
        if(ec)
        {
            fail(ec, "accept");
            continue;
        }

        getManager()->pushPrioritisedEvent<NewWsConnectionEvent>(getServiceId(), _priority, CopyIsMoveWorkaround(std::move(socket)));
    }
}

#endif