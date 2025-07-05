#include <ichor/DependencyManager.h>
#include <ichor/services/network/boost/WsConnectionService.h>
#include <ichor/services/network/ws/WsEvents.h>
#include <ichor/services/network/IHostService.h>
#include <ichor/services/network/http/HttpScopeGuards.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/ScopeGuard.h>
#include <ichor/Filter.h>
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

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService, Ichor::v1::IClientConnectionService>
Ichor::Boost::v1::WsConnectionService<InterfaceT>::WsConnectionService(DependencyRegister &reg, Properties props) : AdvancedService<WsConnectionService<InterfaceT>>(std::move(props)) {
    reg.registerDependency<Ichor::v1::ILogger>(this, DependencyFlags::REQUIRED);
    reg.registerDependency<IBoostAsioQueue>(this, DependencyFlags::REQUIRED);
    if(auto propIt = AdvancedService<WsConnectionService<InterfaceT>>::getProperties().find("WsHostServiceId"); propIt != AdvancedService<WsConnectionService<InterfaceT>>::getProperties().end()) {
        reg.registerDependency<Ichor::v1::IHostService>(this, DependencyFlags::REQUIRED,
                                             Properties{{"Filter", Ichor::v1::make_any<Filter>(ServiceIdFilterEntry{Ichor::v1::any_cast<uint64_t>(propIt->second)})}});
    }
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService, Ichor::v1::IClientConnectionService>
Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::Boost::v1::WsConnectionService<InterfaceT>::start() {
    if(_connected) {
        co_return {};
    }

    _quit = false;

    if(auto propIt = AdvancedService<WsConnectionService<InterfaceT>>::getProperties().find("Priority"); propIt != AdvancedService<WsConnectionService<InterfaceT>>::getProperties().end()) {
        _priority = Ichor::v1::any_cast<uint64_t>(propIt->second);
    }

    _strand = std::make_unique<net::strand<net::io_context::executor_type>>(_queue->getContext().get_executor());
    if (AdvancedService<WsConnectionService<InterfaceT>>::getProperties().contains("Socket")) {
        net::spawn(*_strand, [this](net::yield_context yield) {
            Ichor::v1::ScopeGuardAtomicCount const guard{_finishedListenAndRead};
            accept(std::move(yield));
        }ASIO_SPAWN_COMPLETION_TOKEN);
    } else {
        net::spawn(*_strand, [this](net::yield_context yield) {
            Ichor::v1::ScopeGuardAtomicCount const guard{_finishedListenAndRead};
            connect(std::move(yield));
        }ASIO_SPAWN_COMPLETION_TOKEN);
    }

    co_await _startStopEvent;
    _startStopEvent.reset();

    if(!_connected) {
        auto const& address = Ichor::v1::any_cast<std::string&>(AdvancedService<WsConnectionService<InterfaceT>>::getProperties()["Address"]);
        auto const port = Ichor::v1::any_cast<uint16_t>(AdvancedService<WsConnectionService<InterfaceT>>::getProperties()["Port"]);
        ICHOR_LOG_ERROR(_logger, "Could not connect to {}:{}", address, port);
        co_return tl::unexpected(StartError::FAILED);
    }

    co_return {};
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService, Ichor::v1::IClientConnectionService>
Ichor::Task<void> Ichor::Boost::v1::WsConnectionService<InterfaceT>::stop() {
    INTERNAL_DEBUG("----------------------------------------------- trying to stop WsConnectionService {}", AdvancedService<WsConnectionService<InterfaceT>>::getServiceId());
    _quit = true;
    if(_ws != nullptr) {
        net::spawn(*_strand, [this](net::yield_context yield) {
            Ichor::v1::ScopeGuardAtomicCount const guard{_finishedListenAndRead};
            boost::system::error_code ec;
            _ws->async_close(beast::websocket::close_code::normal, yield[ec]);
            if (ec) {
                ICHOR_LOG_ERROR(_logger, "Boost.BEAST fail: {}", ec.message());
            }
            _startStopEvent.set();
        }ASIO_SPAWN_COMPLETION_TOKEN);

        co_await _startStopEvent;

        while(_finishedListenAndRead.load(std::memory_order_acquire) != 0) {
            _startStopEvent.reset();
            _queue->pushEvent<RunFunctionEvent>(AdvancedService<WsConnectionService<InterfaceT>>::getServiceId(), [this]() {
                _startStopEvent.set();
            });
            co_await _startStopEvent;
        }

        _ws = nullptr;
        _strand = nullptr;
    }

    co_return;
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService, Ichor::v1::IClientConnectionService>
void Ichor::Boost::v1::WsConnectionService<InterfaceT>::addDependencyInstance(Ichor::v1::ILogger &logger, IService &) {
    _logger = &logger;
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService, Ichor::v1::IClientConnectionService>
void Ichor::Boost::v1::WsConnectionService<InterfaceT>::removeDependencyInstance(Ichor::v1::ILogger &logger, IService&) {
    _logger = nullptr;
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService, Ichor::v1::IClientConnectionService>
void Ichor::Boost::v1::WsConnectionService<InterfaceT>::addDependencyInstance(Ichor::v1::IHostService&, IService&) {

}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService, Ichor::v1::IClientConnectionService>
void Ichor::Boost::v1::WsConnectionService<InterfaceT>::removeDependencyInstance(Ichor::v1::IHostService& host, IService&) {
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService, Ichor::v1::IClientConnectionService>
void Ichor::Boost::v1::WsConnectionService<InterfaceT>::addDependencyInstance(IBoostAsioQueue &q, IService&) {
    _queue = &q;
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService, Ichor::v1::IClientConnectionService>
void Ichor::Boost::v1::WsConnectionService<InterfaceT>::removeDependencyInstance(IBoostAsioQueue&, IService&) {
    if(AdvancedService<WsConnectionService<InterfaceT>>::getServiceState() != ServiceState::INSTALLED) {
        std::terminate();
    }
    _queue = nullptr;
}

static_assert(std::is_move_assignable_v<Ichor::Boost::v1::Detail::WsConnectionOutboxMessage>, "ConnectionOutboxMessage should be move assignable");
template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService, Ichor::v1::IClientConnectionService>
Ichor::Task<tl::expected<void, Ichor::v1::IOError>> Ichor::Boost::v1::WsConnectionService<InterfaceT>::sendAsync(std::vector<uint8_t> &&msg) {
    if(_quit || !_connected || _queue->fibersShouldStop()) {
        co_return tl::unexpected(Ichor::v1::IOError::SERVICE_QUITTING);
    }
    AsyncManualResetEvent evt;
    bool success{};
    net::spawn(*_strand, [this, &evt, &success, msg = std::move(msg)](net::yield_context yield) mutable {
        Ichor::v1::ScopeGuardAtomicCount const guard{_finishedListenAndRead};
        if(_quit || !_connected || _queue->fibersShouldStop()) {
            return;
        }

        if(_outbox.full()) {
            _outbox.set_capacity(std::max<uint64_t>(_outbox.capacity() * 2, 10ul));
        }
        _outbox.push_back({std::move(msg), &evt, &success});
        if(_outbox.size() > 1) {
            // handled by existing net::spawn
            return;
        }
        auto ws = _ws;
        while(!_outbox.empty()) {
            auto next = std::move(_outbox.front());

            ScopeGuard const coroutineGuard{[this, event = next.evt]() {
                // use service id 0 to ensure event gets run, even if service is stopped. Otherwise, the coroutine will never complete.
                // Similarly, use priority 0 to ensure these events run before any dependency changes, otherwise the service might be destroyed
                // before we can finish all the coroutines.
                event->set();
                _outbox.pop_front();
            }};

            if(_quit) {
                continue;
            }

            beast::error_code ec;
            ws->async_write(net::const_buffer(next.msg.data(), next.msg.size()), yield[ec]);

            *(next.success) = !ec;
            if(ec) {
                ICHOR_LOG_ERROR(_logger, "couldn't send msg for service {}: {}", AdvancedService<WsConnectionService<InterfaceT>>::getServiceId(), ec.message());
            }
        }

    }ASIO_SPAWN_COMPLETION_TOKEN);

    co_await evt;

    if(!success) {
        co_return tl::unexpected(Ichor::v1::IOError::FAILED);
    }

    co_return {};
}


template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService, Ichor::v1::IClientConnectionService>
Ichor::Task<tl::expected<void, Ichor::v1::IOError>> Ichor::Boost::v1::WsConnectionService<InterfaceT>::sendAsync(std::vector<std::vector<uint8_t>>&& msgs) {
    if(_quit || !_connected || _queue->fibersShouldStop()) {
        co_return tl::unexpected(Ichor::v1::IOError::SERVICE_QUITTING);
    }
    AsyncManualResetEvent evt;
    bool success{};
    net::spawn(*_strand, [this, &evt, &success, msgs = std::move(msgs)](net::yield_context yield) mutable {
        Ichor::v1::ScopeGuardAtomicCount const guard{_finishedListenAndRead};
        if(_quit || !_connected || _queue->fibersShouldStop()) {
            return;
        }

        for(auto &msg : msgs) {
            if (_outbox.full()) {
                _outbox.set_capacity(std::max<uint64_t>(_outbox.capacity() * 2, 10ul));
            }
            _outbox.push_back({std::move(msg), &evt, &success});
        }
        if(_outbox.size() > msgs.size()) {
            // handled by existing net::spawn
            return;
        }
        auto ws = _ws;
        while(!_outbox.empty()) {
            auto next = std::move(_outbox.front());

            ScopeGuard const coroutineGuard{[this, event = next.evt]() {
                // use service id 0 to ensure event gets run, even if service is stopped. Otherwise, the coroutine will never complete.
                // Similarly, use priority 0 to ensure these events run before any dependency changes, otherwise the service might be destroyed
                // before we can finish all the coroutines.
                event->set();
                _outbox.pop_front();
            }};

            if(_quit) {
                continue;
            }

            beast::error_code ec;
            ws->async_write(net::const_buffer(next.msg.data(), next.msg.size()), yield[ec]);

            *(next.success) = !ec;
            if(ec) {
                ICHOR_LOG_ERROR(_logger, "couldn't send msg for service {}: {}", AdvancedService<WsConnectionService<InterfaceT>>::getServiceId(), ec.message());
            }
        }
    }ASIO_SPAWN_COMPLETION_TOKEN);

    co_await evt;

    if(!success) {
        co_return tl::unexpected(Ichor::v1::IOError::FAILED);
    }

    co_return {};
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService, Ichor::v1::IClientConnectionService>
void Ichor::Boost::v1::WsConnectionService<InterfaceT>::setPriority(uint64_t priority) {
    _priority = priority;
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService, Ichor::v1::IClientConnectionService>
uint64_t Ichor::Boost::v1::WsConnectionService<InterfaceT>::getPriority() {
    return _priority;
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService, Ichor::v1::IClientConnectionService>
bool Ichor::Boost::v1::WsConnectionService<InterfaceT>::isClient() const noexcept {
	return AdvancedService<WsConnectionService<InterfaceT>>::getProperties().find("Socket") == AdvancedService<WsConnectionService<InterfaceT>>::getProperties().end();
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService, Ichor::v1::IClientConnectionService>
void Ichor::Boost::v1::WsConnectionService<InterfaceT>::setReceiveHandler(std::function<void(std::span<uint8_t const>)> recvHandler) {
	_recvHandler = recvHandler;

	for(auto &msg : _queuedMessages) {
		_recvHandler(msg);
	}
	_queuedMessages.clear();
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService, Ichor::v1::IClientConnectionService>
void Ichor::Boost::v1::WsConnectionService<InterfaceT>::fail(beast::error_code ec, const char *what) {
    _queue->pushEvent<StopServiceEvent>(AdvancedService<WsConnectionService<InterfaceT>>::getServiceId(), AdvancedService<WsConnectionService<InterfaceT>>::getServiceId());
    ICHOR_LOG_ERROR(_logger, "Boost.BEAST fail: {}, {}", what, ec.message());
    _startStopEvent.set();
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService, Ichor::v1::IClientConnectionService>
void Ichor::Boost::v1::WsConnectionService<InterfaceT>::accept(net::yield_context yield) {
    beast::error_code ec;

    {
        ScopeGuard const coroutineGuard{[this]() {
            _startStopEvent.set();
        }};

        if (!_ws) {
            auto socketIt = AdvancedService<WsConnectionService<InterfaceT>>::getProperties().find("Socket");

            if (socketIt == AdvancedService<WsConnectionService<InterfaceT>>::getProperties().end()) [[unlikely]] {
                return fail(ec, "socket setup");
            }

            _ws = Ichor::v1::any_cast<std::shared_ptr<websocket::stream<beast::tcp_stream>> &>(socketIt->second);
            socketIt->second = Ichor::v1::make_any<bool>(false); // ensure we cannot start again by making a bogus reference to socket
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
        while (!_quit && !_queue->fibersShouldStop() && attempts < 5) {
            // initiate websocket handshake
            _ws->async_accept(yield[ec]);
            if (ec) {
                attempts++;
                net::steady_timer t{_queue->getContext()};
                t.expires_after(250ms);
                t.async_wait(yield);
            } else {
                break;
            }
        }

        if (ec) {
            return fail(ec, "accept");
        }

        _connected = true;
    }

    read(yield);
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService, Ichor::v1::IClientConnectionService>
void Ichor::Boost::v1::WsConnectionService<InterfaceT>::connect(net::yield_context yield) {
    beast::error_code ec;
    auto const& address = Ichor::v1::any_cast<std::string&>(AdvancedService<WsConnectionService<InterfaceT>>::getProperties()["Address"]);
    auto const port = Ichor::v1::any_cast<uint16_t>(AdvancedService<WsConnectionService<InterfaceT>>::getProperties()["Port"]);

    // These objects perform our I/O
    tcp::resolver resolver(_queue->getContext());
    _ws = std::make_shared<websocket::stream<beast::tcp_stream>>(_queue->getContext());

    {
        ScopeGuard const coroutineGuard{[this]() {
            _startStopEvent.set();
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
        while (!_quit && !_queue->fibersShouldStop() && attempts < 5) {
            beast::get_lowest_layer(*_ws).async_connect(results, yield[ec]);
            if (ec) {
                attempts++;
                net::steady_timer t{_queue->getContext()};
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

        // Perform the websocket handshake7
        _ws->async_handshake(address, "/", yield[ec]);
        if (ec) {
            _connected = false;
            return fail(ec, "handshake");
        }
    }

    read(yield);
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService, Ichor::v1::IClientConnectionService>
void Ichor::Boost::v1::WsConnectionService<InterfaceT>::read(net::yield_context &yield) {
    beast::error_code ec;

    while(!_quit && !_queue->fibersShouldStop()) {
        beast::basic_flat_buffer buffer{std::allocator<uint8_t>{}};

        _ws->async_read(buffer, yield[ec]);

        if(_quit || _queue->fibersShouldStop()) {
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
                if(_recvHandler) {
                    _recvHandler(std::span<uint8_t const>(static_cast<uint8_t*>(data.data()), data.size()));
                } else {
                    _queuedMessages.emplace_back(static_cast<uint8_t*>(data.data()), static_cast<uint8_t*>(data.data()) + data.size());
                }
        }
    }

    _connected = false;
    INTERNAL_DEBUG("read stopped WsConnectionService {}", AdvancedService<WsConnectionService<InterfaceT>>::getServiceId());
}

template class Ichor::Boost::v1::WsConnectionService<Ichor::v1::IConnectionService>;
template class Ichor::Boost::v1::WsConnectionService<Ichor::v1::IHostConnectionService>;
template class Ichor::Boost::v1::WsConnectionService<Ichor::v1::IClientConnectionService>;
