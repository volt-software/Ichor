#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/DependencyManager.h>
#include <ichor/services/network/ws/WsConnectionService.h>
#include <ichor/services/network/ws/WsEvents.h>
#include <ichor/services/network/NetworkEvents.h>
#include <ichor/services/network/IHostService.h>

template<class NextLayer>
void setup_stream(std::shared_ptr<websocket::stream<NextLayer>>& ws)
{
    // These values are tuned for Autobahn|Testsuite, and
    // should also be generally helpful for increased performance.

    websocket::permessage_deflate pmd;
    pmd.client_enable = true;
    pmd.server_enable = true;
    pmd.compLevel = 3;
    ws->set_option(pmd);

    ws->auto_fragment(false);
}

Ichor::WsConnectionService::WsConnectionService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
    reg.registerDependency<ILogger>(this, true);
    reg.registerDependency<IHttpContextService>(this, true);
    if(getProperties().contains("WsHostServiceId")) {
        reg.registerDependency<IHostService>(this, true,
                                             Properties{{"Filter", Ichor::make_any<Filter>(ServiceIdFilterEntry{Ichor::any_cast<uint64_t>(getProperties()["WsHostServiceId"])})}});
    }
}

Ichor::AsyncGenerator<void> Ichor::WsConnectionService::start() {
    if(_connected.load(std::memory_order_acquire)) {
        co_return;
    }

    _quit.store(false, std::memory_order_release);

    if (getProperties().contains("Priority")) {
        _priority = Ichor::any_cast<uint64_t>(getProperties()["Priority"]);
    }

    if (getProperties().contains("Socket")) {
        net::spawn(*_httpContextService->getContext(), [this](net::yield_context yield) {
            accept(std::move(yield));
        });
    } else {
        net::spawn(*_httpContextService->getContext(), [this](net::yield_context yield) {
            connect(std::move(yield));
        });
    }

    co_return;
}

Ichor::AsyncGenerator<void> Ichor::WsConnectionService::stop() {
    INTERNAL_DEBUG("----------------------------------------------- trying to stop WsConnectionService {}", getServiceId());
    fmt::print("----------------------------------------------- {}:{} stop\n", getServiceId(), getServiceName());
    _quit.store(true, std::memory_order_release);
    if(_ws != nullptr) {
        _ws->next_layer().close();

        fmt::print("----------------------------------------------- {}:{} wait\n", getServiceId(), getServiceName());
        co_await _startStopEvent;
        fmt::print("----------------------------------------------- {}:{} wait done\n", getServiceId(), getServiceName());

        _ws = nullptr;
    }
    fmt::print("----------------------------------------------- {}:{} stop done\n", getServiceId(), getServiceName());

    co_return;
}

void Ichor::WsConnectionService::addDependencyInstance(ILogger *logger, IService *) {
    _logger = logger;
}

void Ichor::WsConnectionService::removeDependencyInstance(ILogger *logger, IService *) {
    _logger = nullptr;
}

void Ichor::WsConnectionService::addDependencyInstance(IHostService *, IService *) {

}

void Ichor::WsConnectionService::removeDependencyInstance(IHostService *, IService *) {

}

void Ichor::WsConnectionService::addDependencyInstance(IHttpContextService *httpContextService, IService *) {
    _httpContextService = httpContextService;
}

void Ichor::WsConnectionService::removeDependencyInstance(IHttpContextService *logger, IService *) {
    fmt::print("{}:{} set nullptr\n", getServiceId(), getServiceName());
    _httpContextService = nullptr;
}

uint64_t Ichor::WsConnectionService::sendAsync(std::vector<uint8_t> &&msg) {
    if(_quit || _httpContextService->fibersShouldStop()) {
        return false;
    }

    auto id = ++_msgIdCounter;
    net::spawn(*_httpContextService->getContext(), [this, id, msg = std::forward<decltype(msg)>(msg)](net::yield_context yield) mutable {
        if(_quit || _httpContextService->fibersShouldStop()) {
            return;
        }

        beast::error_code ec;
        _ws->write(net::buffer(msg.data(), msg.size()), ec);

        if(ec) {
            ICHOR_LOG_ERROR(_logger, "couldn't send msg for service {}: {}", getServiceId(), ec.message());
            getManager().pushEvent<FailedSendMessageEvent>(getServiceId(), std::move(msg), id);
        }
    });

    return id;
}

void Ichor::WsConnectionService::setPriority(uint64_t priority) {
    _priority.store(priority, std::memory_order_release);
}

uint64_t Ichor::WsConnectionService::getPriority() {
    return _priority.load(std::memory_order_acquire);
}

void Ichor::WsConnectionService::fail(beast::error_code ec, const char *what) {
    fmt::print("{}:{} fail {}\n", getServiceId(), getServiceName(), ec.message());
    ICHOR_LOG_ERROR(_logger, "Boost.BEAST fail: {}, {}", what, ec.message());

    getManager().pushPrioritisedEvent<RunFunctionEvent>(getServiceId(), _priority.load(std::memory_order_acquire), [this](DependencyManager &dm) -> AsyncGenerator<IchorBehaviour> {
        fmt::print("{}:{} rfe\n", getServiceId(), getServiceName());
        _startStopEvent.set();
        co_return {};
    });
    getManager().pushEvent<StopServiceEvent>(getServiceId(), getServiceId());
}

void Ichor::WsConnectionService::accept(net::yield_context yield) {
    beast::error_code ec;

    if(!_ws) {
        auto socketIt = getProperties().find("Socket");

        if(socketIt == getProperties().end()) [[unlikely]] {
            return fail(ec, "socket setup");
        }

        _ws = Ichor::any_cast<std::shared_ptr<websocket::stream<beast::tcp_stream>>&>(socketIt->second);
        socketIt->second = Ichor::make_any<bool>(false); // ensure we cannot start again by making a bogus reference to socket
    }

    setup_stream(_ws);

    // Set suggested timeout settings for the websocket
    _ws->set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));

    // Set a decorator to change the Server of the handshake
    _ws->set_option(websocket::stream_base::decorator(
            [](websocket::response_type &res) {
                res.set(http::field::server, std::string(
                        BOOST_BEAST_VERSION_STRING) + "-Ichor");
            }));

    // Make the connection on the IP address we get from a lookup
    // If it fails (due to connecting earlier than the host is available), wait 250 ms and make another attempt
    // After 5 attempts, fail.
    int attempts{};
    while(!_quit && !_httpContextService->fibersShouldStop() && attempts < 5) {
        // initiate websocket handshake
        _ws->async_accept(yield[ec]);
        if(ec) {
            attempts++;
            net::steady_timer t{*_httpContextService->getContext()};
            t.expires_after(250ms);
            t.async_wait(yield);
        } else {
            break;
        }
    }

    if (ec) {
        return fail(ec, "accept");
    }

    _connected.store(true, std::memory_order_release);

    read(yield);
}

void Ichor::WsConnectionService::connect(net::yield_context yield) {
    beast::error_code ec;
    auto const& address = Ichor::any_cast<std::string&>(getProperties()["Address"]);
    auto const port = Ichor::any_cast<uint16_t>(getProperties()["Port"]);

    // These objects perform our I/O
    tcp::resolver resolver(*_httpContextService->getContext());
    _ws = std::make_unique<websocket::stream<beast::tcp_stream>>(*_httpContextService->getContext());

    // Look up the domain name
    auto const results = resolver.resolve(address, std::to_string(port), ec);
    if(ec) {
        return fail(ec, "resolve");
    }

    // Set a timeout on the operation
    beast::get_lowest_layer(*_ws).expires_after(10s);

    // Make the connection on the IP address we get from a lookup
    // If it fails (due to connecting earlier than the host is available), wait 250 ms and make another attempt
    // After 5 attempts, fail.
    int attempts{};
    while(!_quit && !_httpContextService->fibersShouldStop() && attempts < 5) {
        beast::get_lowest_layer(*_ws).async_connect(results, yield[ec]);
        if(ec) {
            attempts++;
            net::steady_timer t{*_httpContextService->getContext()};
            t.expires_after(std::chrono::milliseconds(250));
            t.async_wait(yield);
        } else {
            break;
        }
    }


    if (ec) {
        return fail(ec, "connect");
    }

    _connected.store(true, std::memory_order_release);

    // Turn off the timeout on the tcp_stream, because
    // the websocket stream has its own timeout system.
    beast::get_lowest_layer(*_ws).expires_never();

    // Set suggested timeout settings for the websocket
    _ws->set_option(
            websocket::stream_base::timeout::suggested(
                    beast::role_type::client));

    // Set a decorator to change the User-Agent of the handshake
    _ws->set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req)
            {
                req.set(http::field::user_agent,
                        std::string(BOOST_BEAST_VERSION_STRING) +
                        " ichor");
            }));

    // Perform the websocket handshake
    _ws->async_handshake(address, "/", yield[ec]);
    if(ec) {
        _connected = false;
        return fail(ec, "handshake");
    }

    read(yield);
}

void Ichor::WsConnectionService::read(net::yield_context &yield) {
    beast::error_code ec;

    while(!_quit.load(std::memory_order_acquire) && !_httpContextService->fibersShouldStop()) {
        fmt::print("{}:{} read\n", getServiceId(), getServiceName());
        beast::basic_flat_buffer buffer{std::allocator<uint8_t>{}};

        _ws->async_read(buffer, yield[ec]);
        fmt::print("{}:{} read post async_read\n", getServiceId(), getServiceName());

        if(_quit.load(std::memory_order_acquire) || _httpContextService->fibersShouldStop()) {
            break;
        }

        if(ec == websocket::error::closed) {
            break;
        }
        if(ec) {
            _connected = false;
            return fail(ec, "read");
        }

        if(_ws->got_text()) {
            auto data = buffer.data();
            getManager().pushPrioritisedEvent<NetworkDataEvent>(getServiceId(), _priority.load(std::memory_order_acquire),  std::vector<uint8_t>{static_cast<char*>(data.data()), static_cast<char*>(data.data()) + data.size()});
        }
    }

    _connected = false;
    INTERNAL_DEBUG("read stopped WsConnectionService {}", getServiceId());
    fmt::print("{}:{} read done\n", getServiceId(), getServiceName());

    getManager().pushPrioritisedEvent<RunFunctionEvent>(getServiceId(), _priority.load(std::memory_order_acquire), [this](DependencyManager &dm) -> AsyncGenerator<IchorBehaviour> {
        fmt::print("{}:{} rfe2\n", getServiceId(), getServiceName());
        _startStopEvent.set();
        co_return {};
    });
}

#endif