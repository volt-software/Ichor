#ifdef USE_BOOST_BEAST

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/network_bundle/ws/WsConnectionService.h>
#include <ichor/optional_bundles/network_bundle/ws/WsEvents.h>
#include <ichor/optional_bundles/network_bundle/ws/WsCopyIsMoveWorkaround.h>
#include <ichor/optional_bundles/network_bundle/NetworkDataEvent.h>
#include <ichor/optional_bundles/network_bundle/IHostService.h>

template<class NextLayer>
void setup_stream(std::unique_ptr<websocket::stream<NextLayer>>& ws)
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
    if(props.contains("WsHostServiceId")) {
        reg.registerDependency<IHostService>(this, true,
                                             Ichor::make_properties(getMemoryResource(),
                                             IchorProperty{"Filter", Ichor::make_any<Filter>(getMemoryResource(), getMemoryResource(), ServiceIdFilterEntry{Ichor::any_cast<uint64_t>(props["WsHostServiceId"])})}));
    }
}

bool Ichor::WsConnectionService::start() {
    if(_quit.load(std::memory_order_acquire)) {
        return false;
    }

    if(!_connecting && !_connected) {
        _connecting = true;

        if (getProperties()->contains("Priority")) {
            _priority = Ichor::any_cast<uint64_t>(getProperties()->operator[]("Priority"));
        }

        if (getProperties()->contains("Socket")) {
            _sendTimer = std::make_unique<net::steady_timer>(Ichor::any_cast<net::executor>(getProperties()->operator[]("Executor")));
            net::spawn(Ichor::any_cast<net::executor>(getProperties()->operator[]("Executor")), [this](net::yield_context yield) {
                accept(std::move(yield));
            });

            net::spawn(Ichor::any_cast<net::executor>(getProperties()->operator[]("Executor")), [this](net::yield_context yield) {
                sendStrand(std::move(yield));
            });
        } else {
            _wsContext = std::make_unique<net::io_context>(1);
            _sendTimer = std::make_unique<net::steady_timer>(*_wsContext);

            net::spawn(*_wsContext, [this](net::yield_context yield) {
                connect(std::move(yield));
            });

            net::spawn(*_wsContext, [this](net::yield_context yield) {
                sendStrand(std::move(yield));
            });

            _connectThread = std::thread([this] {
                _wsContext->run();
            });

#ifdef __linux__
            pthread_setname_np(_connectThread.native_handle(), fmt::format("WsConn #{}", getServiceId()).c_str());
#endif
        }
    }

    if(!_connected) {
        return false;
    }

    return true;
}

bool Ichor::WsConnectionService::stop() {
    ICHOR_LOG_TRACE(_logger, "trying to stop WsConnectionService {}", getServiceId());
    bool expected = false;
    bool stopWsContext = false;
    if(_quit.compare_exchange_strong(expected, true)) {
        try {
            ICHOR_LOG_TRACE(_logger, "ws next layer close WsConnectionService {}", getServiceId());
            _ws->next_layer().close();
            cancelSendTimer();
        } catch (...) {
            // ignore
        }
        stopWsContext = true;
    }

    if(_connectThread.joinable()) {
        _connectThread.join();
    }

    while(!_sendStrandDone) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (stopWsContext && _wsContext) {
        _wsContext->stop();
    }

    _sendTimer = nullptr;
    _ws = nullptr;
    _wsContext = nullptr;

    return true;
}

void Ichor::WsConnectionService::addDependencyInstance(ILogger *logger, IService *) {
    _logger = logger;
    ICHOR_LOG_TRACE(_logger, "Inserted logger");
}

void Ichor::WsConnectionService::removeDependencyInstance(ILogger *logger, IService *) {
    _logger = nullptr;
}

void Ichor::WsConnectionService::addDependencyInstance(IHostService *, IService *) {

}

void Ichor::WsConnectionService::removeDependencyInstance(IHostService *, IService *) {

}

bool Ichor::WsConnectionService::send(std::pmr::vector<uint8_t> &&msg) {
    if(_quit.load(std::memory_order_acquire)) {
        return false;
    }

    {
        std::scoped_lock l(_queueMutex);
        _msgQueue.push(std::forward<decltype(msg)>(msg));
    }

    cancelSendTimer();

    return true;
}

void Ichor::WsConnectionService::setPriority(uint64_t priority) {
    _priority.store(priority, std::memory_order_release);
}

uint64_t Ichor::WsConnectionService::getPriority() {
    return _priority.load(std::memory_order_acquire);
}

void Ichor::WsConnectionService::fail(beast::error_code ec, const char *what) {
    ICHOR_LOG_ERROR(_logger, "Boost.BEAST fail: {}, {}", what, ec.message());
    getManager()->pushEvent<StopServiceEvent>(getServiceId(), getServiceId());
}

void Ichor::WsConnectionService::sendStrand(net::yield_context yield) {
    while(!_quit.load(std::memory_order_acquire)) {
        beast::error_code ec;

        {
            std::unique_lock l(_queueMutex);
            if (!_msgQueue.empty()) {
                auto msg = _msgQueue.front();
                l.unlock();
                _ws->async_write(net::buffer(msg.data(), msg.size()), yield[ec]);

                while(!_quit.load(std::memory_order_acquire) && ec && ec != websocket::error::closed) {
                    _sendTimer->expires_after(std::chrono::milliseconds (5));
                    _sendTimer->async_wait(yield[ec]);
                    _ws->async_write(net::buffer(msg.data(), msg.size()), yield[ec]);
                }

                if(ec == websocket::error::closed) {
                    break;
                }

                l.lock();
                _msgQueue.pop();
            }
        }

        if(!_quit.load(std::memory_order_acquire)) {
            _sendTimer->expires_after(std::chrono::milliseconds(50));
            _sendTimer->async_wait(yield[ec]);
        }
    }

    _sendStrandDone = true;
}

void Ichor::WsConnectionService::accept(net::yield_context yield) {
    beast::error_code ec;

    if(!_ws) {
        auto &socket = Ichor::any_cast<CopyIsMoveWorkaround<tcp::socket>&>(getProperties()->operator[]("Socket"));
        _ws = std::make_unique<websocket::stream<beast::tcp_stream>>(socket.moveObject());
    }

    setup_stream(_ws);

    // Set suggested timeout settings for the websocket
    _ws->set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));

    // Set a decorator to change the Server of the handshake
    _ws->set_option(websocket::stream_base::decorator(
            [](websocket::response_type &res) {
                res.set(http::field::server, std::string(
                        BOOST_BEAST_VERSION_STRING) + "-Fiber");
            }));

    // Make the connection on the IP address we get from a lookup
    // If it fails (due to connecting earlier than the host is available), wait 250 ms and make another attempt
    // After 5 attempts, fail.
    while(!_quit && _attempts < 5) {
        // initiate websocket handshake
        _ws->async_accept(yield[ec]);
        if(ec) {
            _attempts++;
            net::steady_timer t{Ichor::any_cast<net::executor>(getProperties()->operator[]("Executor"))};
            t.expires_after(std::chrono::milliseconds(250));
            t.async_wait(yield);
        } else {
            break;
        }
    }

    if (ec) {
        return fail(ec, "accept");
    }
    _connected = true;
    _connecting = false;

    getManager()->pushEvent<StartServiceEvent>(getServiceId(), getServiceId());

    read(yield);
}

void Ichor::WsConnectionService::connect(net::yield_context yield) {
    beast::error_code ec;
    auto const& address = Ichor::any_cast<std::string&>(getProperties()->operator[]("Address"));
    auto const port = Ichor::any_cast<uint16_t>(getProperties()->operator[]("Port"));

    // These objects perform our I/O
    tcp::resolver resolver(*_wsContext);
    _ws = std::make_unique<websocket::stream<beast::tcp_stream>>(*_wsContext);

    // Look up the domain name
    auto const results = resolver.async_resolve(address, std::to_string(port), yield[ec]);
    if(ec) {
        return fail(ec, "resolve");
    }

    // Set a timeout on the operation
    beast::get_lowest_layer(*_ws).expires_after(std::chrono::seconds(10));

    // Make the connection on the IP address we get from a lookup
    // If it fails (due to connecting earlier than the host is available), wait 250 ms and make another attempt
    // After 5 attempts, fail.
    while(!_quit && _attempts < 5) {
        beast::get_lowest_layer(*_ws).async_connect(results, yield[ec]);
        if(ec) {
            _attempts++;
            net::steady_timer t{*_wsContext};
            t.expires_after(std::chrono::milliseconds(250));
            t.async_wait(yield);
        } else {
            break;
        }
    }


    if (ec) {
        return fail(ec, "connect");
    }

    _connected = true;
    _connecting = false;

    getManager()->pushEvent<StartServiceEvent>(getServiceId(), getServiceId());

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
                        " websocket-client-coro");
            }));

    // Perform the websocket handshake
    _ws->async_handshake(address, "/", yield[ec]);
    if(ec) {
        return fail(ec, "handshake");
    }

    read(yield);
}

void Ichor::WsConnectionService::read(net::yield_context &yield) {
    beast::error_code ec;

    while(!_quit.load(std::memory_order_acquire))
    {
        beast::flat_buffer buffer;

        _ws->async_read(buffer, yield[ec]);
        if(ec == websocket::error::closed) {
            break;
        }
        if(ec) {
            return fail(ec, "read");
        }

        if(_ws->got_text()) {
            auto data = buffer.data();
            getManager()->pushPrioritisedEvent<NetworkDataEvent>(getServiceId(), _priority.load(std::memory_order_acquire),  std::pmr::vector<uint8_t>{static_cast<char*>(data.data()), static_cast<char*>(data.data()) + data.size(), getMemoryResource()});
        }
    }
}

void Ichor::WsConnectionService::cancelSendTimer() {
    if(_wsContext) {
        ICHOR_LOG_TRACE(_logger, "cancelSendTimer wsContext->post() WsConnectionService {}", getServiceId());
        _wsContext->post([this](){
            _sendTimer->cancel();
        });
    } else {
        ICHOR_LOG_TRACE(_logger, "cancelSendTimer net::spawn WsConnectionService {}", getServiceId());
        net::spawn(Ichor::any_cast<net::executor>(getProperties()->operator[]("Executor")), [this](net::yield_context yield) {
            _sendTimer->cancel();
        });
    }
}

#endif