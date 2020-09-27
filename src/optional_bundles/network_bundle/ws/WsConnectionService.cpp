#ifdef USE_BOOST_BEAST

#include <cppelix/DependencyManager.h>
#include <cppelix/optional_bundles/network_bundle/ws/WsConnectionService.h>
#include <cppelix/optional_bundles/network_bundle/ws/WsEvents.h>
#include <cppelix/optional_bundles/network_bundle/ws/WsCopyIsMoveWorkaround.h>
#include <cppelix/optional_bundles/network_bundle/NetworkDataEvent.h>

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

Cppelix::WsConnectionService::WsConnectionService(DependencyRegister &reg, CppelixProperties props) : Service(std::move(props)) {
    reg.registerDependency<ILogger>(this, true);
}

bool Cppelix::WsConnectionService::start() {
    if(_quit) {
        return false;
    }

    if(!_connecting && !_connected) {
        if (getProperties()->contains("Priority")) {
            _priority = std::any_cast<uint64_t>(getProperties()->operator[]("Priority"));
        }

        if (getProperties()->contains("Socket")) {
            net::spawn(std::any_cast<net::executor>(getProperties()->operator[]("Executor")), [this](net::yield_context yield) {
                beast::error_code ec;

                {
                    auto socket = std::any_cast<CopyIsMoveWorkaround<tcp::socket>>(getProperties()->operator[]("Socket"));
                    _ws = std::make_unique<websocket::stream<beast::tcp_stream>>(std::move(socket._obj));
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
                while(_attempts < 5) {
                    // initiate websocket handshake
                    _ws->async_accept(yield[ec]);
                    if(ec) {
                        _attempts++;
                        net::steady_timer t{std::any_cast<net::executor>(getProperties()->operator[]("Executor"))};
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

                read(yield);
            });
        } else {
            _wsContext = std::make_unique<net::io_context>(1);

            net::spawn(*_wsContext, [this](net::yield_context yield) {
                connect(std::move(yield));
            });

            _connectThread = std::thread([this] {
                _wsContext->run();
            });
        }

        _connecting = true;
    }

    if(!_connected && _attempts < 5) {
//        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        // cannot use priority here, would possibly block the loop
        getManager()->pushEvent<StartServiceEvent>(getServiceId(), getServiceId());
        return false;
    } else if (!_connected && _attempts >= 5) {
        return false;
    }

    return true;
}

bool Cppelix::WsConnectionService::stop() {
    bool expected = false;
    if(_quit.compare_exchange_strong(expected, true)) {
        if (_wsContext) {
            _wsContext->stop();
        } else {
            _ws->close(websocket::close_code::normal);
        }
    }

    if(_connectThread.joinable()) {
        _connectThread.join();
    }

    _ws = nullptr;
    _wsContext = nullptr;

    return true;
}

void Cppelix::WsConnectionService::addDependencyInstance(ILogger *logger) {
    _logger = logger;
    LOG_TRACE(_logger, "Inserted logger");
}

void Cppelix::WsConnectionService::removeDependencyInstance(ILogger *logger) {
    _logger = nullptr;
}

void Cppelix::WsConnectionService::send(std::vector<uint8_t> &&msg) {
    _ws->write(net::buffer(msg.data(), msg.size()));
//    if(getProperties()->contains("Socket")) {
//        net::spawn(std::any_cast<net::executor>(getProperties()->operator[]("Executor")), [this, msg = std::forward<std::vector<uint8_t>>(msg)](net::yield_context yield){
//            beast::error_code ec;
//
//            _ws->async_write(net::buffer(msg.data(), msg.size()), yield[ec]);
//            if(ec) {
//                return fail(ec, "write");
//            }
//        });
//    } else {
//        net::spawn(*_wsContext, [this, msg = std::forward<std::vector<uint8_t>>(msg)](net::yield_context yield){
//            beast::error_code ec;
//
//            _ws->async_write(net::buffer(msg.data(), msg.size()), yield[ec]);
//            if(ec) {
//                return fail(ec, "write");
//            }
//        });
//    }
}

void Cppelix::WsConnectionService::setPriority(uint64_t priority) {
    _priority.store(priority, std::memory_order_release);
}

uint64_t Cppelix::WsConnectionService::getPriority() {
    return _priority.load(std::memory_order_acquire);
}

void Cppelix::WsConnectionService::fail(beast::error_code ec, const char *what) {
    LOG_ERROR(_logger, "Boost.BEAST fail: {}, {}", what, ec.message());
}

void Cppelix::WsConnectionService::connect(net::yield_context yield) {
    beast::error_code ec;
    auto const address = std::any_cast<std::string&>(getProperties()->operator[]("Address"));
    auto const port = std::any_cast<uint16_t>(getProperties()->operator[]("Port"));

    // These objects perform our I/O
    tcp::resolver resolver(*_wsContext);
    _ws = std::make_unique<websocket::stream<beast::tcp_stream>>(*_wsContext);

    // Look up the domain name
    auto const results = resolver.async_resolve(address, std::to_string(port), yield[ec]);
    if(ec) {
        return fail(ec, "resolve");
    }

    // Set a timeout on the operation
    beast::get_lowest_layer(*_ws).expires_after(std::chrono::seconds(30));

    // Make the connection on the IP address we get from a lookup
    // If it fails (due to connecting earlier than the host is available), wait 250 ms and make another attempt
    // After 5 attempts, fail.
    while(_attempts < 5) {
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

    // Close the WebSocket connection
    _ws->async_close(websocket::close_code::normal, yield[ec]);
    if(ec) {
        return fail(ec, "close");
    }

    // If we get here then the connection is closed gracefully
}

void Cppelix::WsConnectionService::read(net::yield_context &yield) {
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
            getManager()->pushPrioritisedEvent<NetworkDataEvent>(getServiceId(), _priority.load(std::memory_order_acquire),  std::vector<uint8_t>{static_cast<char*>(data.data()), static_cast<char*>(data.data()) + data.size()});
        }
    }
}

#endif