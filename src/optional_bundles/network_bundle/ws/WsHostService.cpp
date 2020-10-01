#ifdef USE_BOOST_BEAST

#include <cppelix/DependencyManager.h>
#include <cppelix/optional_bundles/network_bundle/IConnectionService.h>
#include <cppelix/optional_bundles/network_bundle/ws/WsHostService.h>
#include <cppelix/optional_bundles/network_bundle/ws/WsConnectionService.h>
#include <cppelix/optional_bundles/network_bundle/ws/WsEvents.h>
#include <cppelix/optional_bundles/network_bundle/NetworkDataEvent.h>

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


Cppelix::WsHostService::WsHostService(DependencyRegister &reg, CppelixProperties props) : Service(std::move(props)) {
    reg.registerDependency<ILogger>(this, true);
}

bool Cppelix::WsHostService::start() {
    if(getProperties()->contains("Priority")) {
        _priority = std::any_cast<uint64_t>(getProperties()->operator[]("Priority"));
    }

    if(!getProperties()->contains("Port") || !getProperties()->contains("Address")) {
        getManager()->pushPrioritisedEvent<UnrecoverableErrorEvent>(getServiceId(), _priority, 0, "Missing port or address when starting WsHostService");
        return false;
    }

    _eventRegistration = getManager()->registerEventHandler<NewWsConnectionEvent>(getServiceId(), this, getServiceId());

    _wsContext = std::make_unique<net::io_context>(1);
    auto address = net::ip::make_address(std::any_cast<std::string&>(getProperties()->operator[]("Address")));
    auto port = std::any_cast<uint16_t>(getProperties()->operator[]("Port"));

    net::spawn(*_wsContext, [this, address = std::move(address), port](net::yield_context yield){
        listen(tcp::endpoint{address, port}, std::move(yield));
    });

    _listenThread = std::thread([this]{
        _wsContext->run();
    });

#ifdef __linux__
    pthread_setname_np(_listenThread.native_handle(), fmt::format("WsHost #{}", getServiceId()).c_str());
#endif

    return true;
}

bool Cppelix::WsHostService::stop() {
    _quit = true;

    for(auto conn : _connections) {
        conn->stop();
    }

    _wsAcceptor->close();

    _listenThread.join();

    _wsContext->stop();
    _wsContext = nullptr;

    return true;
}

void Cppelix::WsHostService::addDependencyInstance(ILogger *logger) {
    _logger = logger;
    LOG_TRACE(_logger, "Inserted logger");
}

void Cppelix::WsHostService::removeDependencyInstance(ILogger *logger) {
    _logger = nullptr;
}

Cppelix::Generator<bool> Cppelix::WsHostService::handleEvent(Cppelix::NewWsConnectionEvent const * const evt) {
    auto connection = getManager()->createServiceManager<WsConnectionService, IConnectionService>(CppelixProperties{
        {"WsHostServiceId", getServiceId()},
        {"Socket", evt->_socket},
        {"Executor", evt->_executor}
    });
    _connections.push_back(connection);

    co_return (bool)PreventOthersHandling;
}

void Cppelix::WsHostService::setPriority(uint64_t priority) {
    _priority.store(priority, std::memory_order_release);
}

uint64_t Cppelix::WsHostService::getPriority() {
    return _priority.load(std::memory_order_acquire);
}

void Cppelix::WsHostService::fail(beast::error_code ec, const char *what) {
    LOG_ERROR(_logger, "Boost.BEAST fail: {}, {}", what, ec.message());
    getManager()->pushPrioritisedEvent<StopServiceEvent>(getServiceId(), _priority, getServiceId());
}

void Cppelix::WsHostService::listen(tcp::endpoint endpoint, net::yield_context yield)
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
        auto socket = tcp::socket(*_wsContext);

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