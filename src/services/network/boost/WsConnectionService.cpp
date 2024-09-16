#include <ichor/DependencyManager.h>
#include <ichor/services/network/boost/WsConnectionService.h>
#include <ichor/services/network/ws/WsEvents.h>
#include <ichor/services/network/NetworkEvents.h>
#include <ichor/services/network/IHostService.h>
#include <ichor/services/network/boost/AsioContextService.h>
#include <ichor/services/network/http/HttpScopeGuards.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/ScopeGuard.h>
#include <thread>

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

Ichor::WsConnectionService::WsConnectionService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
    reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
    reg.registerDependency<IAsioContextService>(this, DependencyFlags::REQUIRED);
    if(auto propIt = getProperties().find("WsHostServiceId"); propIt != getProperties().end()) {
        reg.registerDependency<IHostService>(this, DependencyFlags::REQUIRED,
                                             Properties{{"Filter", Ichor::make_any<Filter>(ServiceIdFilterEntry{Ichor::any_cast<uint64_t>(propIt->second)})}});
    }
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::WsConnectionService::start() {
    if(_connected.load(std::memory_order_acquire)) {
        co_return {};
    }

    _queue = &GetThreadLocalEventQueue();

    _quit.store(false, std::memory_order_release);

    if(auto propIt = getProperties().find("Priority"); propIt != getProperties().end()) {
        _priority = Ichor::any_cast<uint64_t>(propIt->second);
    }

    _strand = std::make_unique<net::strand<net::io_context::executor_type>>(_asioContextService->getContext()->get_executor());
    if (getProperties().contains("Socket")) {
        net::spawn(*_strand, [this](net::yield_context yield) {
            ScopeGuardAtomicCount const guard{_finishedListenAndRead};
            accept(std::move(yield));
        }ASIO_SPAWN_COMPLETION_TOKEN);
    } else {
        net::spawn(*_strand, [this](net::yield_context yield) {
            ScopeGuardAtomicCount const guard{_finishedListenAndRead};
            connect(std::move(yield));
        }ASIO_SPAWN_COMPLETION_TOKEN);
    }

    co_await _startStopEvent;
    _startStopEvent.reset();

    if(!_connected.load(std::memory_order_acquire)) {
        auto const& address = Ichor::any_cast<std::string&>(getProperties()["Address"]);
        auto const port = Ichor::any_cast<uint16_t>(getProperties()["Port"]);
        ICHOR_LOG_ERROR(_logger, "Could not connect to {}:{}", address, port);
        co_return tl::unexpected(StartError::FAILED);
    }
//    fmt::print("----------------------------------------------- {}:{} start done\n", getServiceId(), getServiceName());

    co_return {};
}

Ichor::Task<void> Ichor::WsConnectionService::stop() {
//    INTERNAL_DEBUG("----------------------------------------------- trying to stop WsConnectionService {}", getServiceId());
//    fmt::print("----------------------------------------------- {}:{} stop\n", getServiceId(), getServiceName());
    _quit.store(true, std::memory_order_release);
    if(_ws != nullptr) {
        net::spawn(*_strand, [this](net::yield_context yield) {
            ScopeGuardAtomicCount const guard{_finishedListenAndRead};
            boost::system::error_code ec;
//            fmt::print("----------------------------------------------- {}:{} async_close\n", getServiceId(), getServiceName());
            _ws->async_close(beast::websocket::close_code::normal, yield[ec]);
//            fmt::print("----------------------------------------------- {}:{} async_close done\n", getServiceId(), getServiceName());
            if (ec) {
                ICHOR_LOG_ERROR(_logger, "Boost.BEAST fail: {}", ec.message());
            }
            _queue->pushPrioritisedEvent<RunFunctionEvent>(getServiceId(), _priority.load(std::memory_order_acquire), [this]() {
//                fmt::print("{}:{} rfe2\n", getServiceId(), getServiceName());
                _startStopEvent.set();
            });
        }ASIO_SPAWN_COMPLETION_TOKEN);

//        fmt::print("----------------------------------------------- {}:{} wait {}\n", getServiceId(), getServiceName(), _startStopEvent.is_set());
        co_await _startStopEvent;
//        fmt::print("----------------------------------------------- {}:{} wait done\n", getServiceId(), getServiceName());

        while(_finishedListenAndRead.load(std::memory_order_acquire) != 0) {
            std::this_thread::sleep_for(1ms);
        }

        _ws = nullptr;
        _strand = nullptr;
    }
//    fmt::print("----------------------------------------------- {}:{} stop done\n", getServiceId(), getServiceName());

    co_return;
}

void Ichor::WsConnectionService::addDependencyInstance(ILogger &logger, IService &) {
    _logger = &logger;
}

void Ichor::WsConnectionService::removeDependencyInstance(ILogger &logger, IService&) {
    _logger = nullptr;
}

void Ichor::WsConnectionService::addDependencyInstance(IHostService&, IService&) {

}

void Ichor::WsConnectionService::removeDependencyInstance(IHostService& host, IService&) {
}

void Ichor::WsConnectionService::addDependencyInstance(IAsioContextService &AsioContextService, IService&) {
    _asioContextService = &AsioContextService;
}

void Ichor::WsConnectionService::removeDependencyInstance(IAsioContextService&, IService&) {
//    fmt::print("{}:{} {} set nullptr\n", getServiceId(), getServiceName(), getServiceState());
    if(getServiceState() != ServiceState::INSTALLED) {
        std::terminate();
    }
    _asioContextService = nullptr;
}

Ichor::Task<tl::expected<uint64_t, Ichor::IOError>> Ichor::WsConnectionService::sendAsync(std::vector<uint8_t> &&msg) {
    static_assert(std::is_move_assignable_v<Detail::WsConnectionOutboxMessage>, "ConnectionOutboxMessage should be move assignable");
    if(_quit.load(std::memory_order_acquire) || !_connected.load(std::memory_order_acquire) || _asioContextService->fibersShouldStop()) {
        co_return tl::unexpected(IOError::SERVICE_QUITTING);
    }

    auto id = ++_msgIdCounter;
    net::spawn(*_strand, [this, id, msg = std::move(msg)](net::yield_context yield) mutable {
        ScopeGuardAtomicCount const guard{_finishedListenAndRead};
        if(_quit.load(std::memory_order_acquire) || !_connected.load(std::memory_order_acquire) || _asioContextService->fibersShouldStop()) {
            return;
        }

        if(_outbox.full()) {
            _outbox.set_capacity(std::max<uint64_t>(_outbox.capacity() * 2, 10ul));
        }
        _outbox.push_back({id, std::move(msg)});
        if(_outbox.size() > 1) {
            // handled by existing net::spawn
            return;
        }
        auto ws = _ws;
        while(!_outbox.empty()) {
            auto next = std::move(_outbox.front());

            ScopeGuard const coroutineGuard{[this]() {
                _outbox.pop_front();
            }};

            if(_quit.load(std::memory_order_acquire)) {
                continue;
            }

            beast::error_code ec;
            ws->async_write(net::const_buffer(next.msg.data(), next.msg.size()), yield[ec]);

            if(ec) {
                ICHOR_LOG_ERROR(_logger, "couldn't send msg for service {}: {}", getServiceId(), ec.message());
                _queue->pushEvent<FailedSendMessageEvent>(getServiceId(), std::move(next.msg), next.msgId);
            }
        }
    }ASIO_SPAWN_COMPLETION_TOKEN);

    co_return id;
}

void Ichor::WsConnectionService::setPriority(uint64_t priority) {
    _priority.store(priority, std::memory_order_release);
}

uint64_t Ichor::WsConnectionService::getPriority() {
    return _priority.load(std::memory_order_acquire);
}

bool Ichor::WsConnectionService::isClient() const noexcept {
	return getProperties().find("Socket") == getProperties().end();
}

void Ichor::WsConnectionService::setReceiveHandler(std::function<void(std::span<uint8_t const>)> recvHandler) {
	_recvHandler = recvHandler;

	for(auto &msg : _queuedMessages) {
		_recvHandler(msg);
	}
	_queuedMessages.clear();
}

void Ichor::WsConnectionService::fail(beast::error_code ec, const char *what) {
//    fmt::print("{}:{} fail {}\n", getServiceId(), getServiceName(), ec.message());
    ICHOR_LOG_ERROR(_logger, "Boost.BEAST fail: {}, {}", what, ec.message());

    _queue->pushPrioritisedEvent<RunFunctionEvent>(getServiceId(), _priority.load(std::memory_order_acquire), [this]() {
//        fmt::print("{}:{} rfe\n", getServiceId(), getServiceName());
        _startStopEvent.set();
    });
    _queue->pushEvent<StopServiceEvent>(getServiceId(), getServiceId());
}

void Ichor::WsConnectionService::accept(net::yield_context yield) {
    beast::error_code ec;

    {
        ScopeGuard const coroutineGuard{[this]() {
            _queue->pushPrioritisedEvent<RunFunctionEvent>(getServiceId(), _priority.load(std::memory_order_acquire), [this]() {
//                fmt::print("{}:{} rfe accept\n", getServiceId(), getServiceName());
                _startStopEvent.set();
            });
        }};

        if (!_ws) {
            auto socketIt = getProperties().find("Socket");

            if (socketIt == getProperties().end()) [[unlikely]] {
                return fail(ec, "socket setup");
            }

            _ws = Ichor::any_cast<std::shared_ptr<websocket::stream<beast::tcp_stream>> &>(socketIt->second);
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
        while (!_quit.load(std::memory_order_acquire) && !_asioContextService->fibersShouldStop() && attempts < 5) {
            // initiate websocket handshake
            _ws->async_accept(yield[ec]);
            if (ec) {
                attempts++;
                net::steady_timer t{*_asioContextService->getContext()};
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
    }

    read(yield);
}

void Ichor::WsConnectionService::connect(net::yield_context yield) {
    beast::error_code ec;
    auto const& address = Ichor::any_cast<std::string&>(getProperties()["Address"]);
    auto const port = Ichor::any_cast<uint16_t>(getProperties()["Port"]);

    // These objects perform our I/O
    tcp::resolver resolver(*_asioContextService->getContext());
    _ws = std::make_shared<websocket::stream<beast::tcp_stream>>(*_asioContextService->getContext());

    {
        ScopeGuard const coroutineGuard{[this]() {
            _queue->pushPrioritisedEvent<RunFunctionEvent>(getServiceId(), _priority.load(std::memory_order_acquire), [this]() {
//                fmt::print("{}:{} rfe connect\n", getServiceId(), getServiceName());
                _startStopEvent.set();
            });
        }};

        // Look up the domain name
        auto const results = resolver.resolve(address, std::to_string(port), ec);
        if (ec) {
            return fail(ec, "resolve");
        }

        // Set a timeout on the operation
        beast::get_lowest_layer(*_ws).expires_after(10s);

        // Make the connection on the IP address we get from a lookup
        // If it fails (due to connecting earlier than the host is available), wait 250 ms and make another attempt
        // After 5 attempts, fail.
        int attempts{};
        while (!_quit.load(std::memory_order_acquire) && !_asioContextService->fibersShouldStop() && attempts < 5) {
            beast::get_lowest_layer(*_ws).async_connect(results, yield[ec]);
            if (ec) {
                attempts++;
                net::steady_timer t{*_asioContextService->getContext()};
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
                [](websocket::request_type &req) {
                    req.set(http::field::user_agent,
                            std::string(BOOST_BEAST_VERSION_STRING) +
                            " ichor");
                }));

        // Perform the websocket handshake
        _ws->async_handshake(address, "/", yield[ec]);
        if (ec) {
            _connected.store(false, std::memory_order_release);
            return fail(ec, "handshake");
        }
    }

    read(yield);
}

void Ichor::WsConnectionService::read(net::yield_context &yield) {
    beast::error_code ec;

    while(!_quit.load(std::memory_order_acquire) && !_asioContextService->fibersShouldStop()) {
//        fmt::print("{}:{} read\n", getServiceId(), getServiceName());
        beast::basic_flat_buffer buffer{std::allocator<uint8_t>{}};

        _ws->async_read(buffer, yield[ec]);
//        fmt::print("{}:{} read post async_read\n", getServiceId(), getServiceName());

        if(_quit.load(std::memory_order_acquire) || _asioContextService->fibersShouldStop()) {
            break;
        }

        if(ec == websocket::error::closed) {
            break;
        }
        if(ec) {
            _connected.store(false, std::memory_order_release);
            return fail(ec, "read");
        }

        if(_ws->got_text()) {
            auto data = buffer.data();
            _queue->pushPrioritisedEvent<RunFunctionEvent>(getServiceId(), _priority.load(std::memory_order_acquire), [this, data = std::vector<uint8_t>{static_cast<uint8_t*>(data.data()), static_cast<uint8_t*>(data.data()) + data.size()}]() mutable {
                if(_recvHandler) {
                    _recvHandler(data);
                } else {
                    _queuedMessages.emplace_back(std::move(data));
                }
            });
        }
    }

    _connected.store(false, std::memory_order_release);
    INTERNAL_DEBUG("read stopped WsConnectionService {}", getServiceId());
//    fmt::print("{}:{} read done\n", getServiceId(), getServiceName());
}
