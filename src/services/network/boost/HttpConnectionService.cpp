#include <ichor/DependencyManager.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/services/network/boost/HttpConnectionService.h>
#include <ichor/services/network/http/HttpScopeGuards.h>
#include <ichor/services/network/NetworkEvents.h>
#include <ichor/ScopeGuard.h>

Ichor::HttpConnectionService::HttpConnectionService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
    reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
    reg.registerDependency<IAsioContextService>(this, DependencyFlags::REQUIRED);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::HttpConnectionService::start() {
    _queue = &GetThreadLocalEventQueue();

    if (!_asioContextService->fibersShouldStop() && !_connecting.load(std::memory_order_acquire) && !_connected.load(std::memory_order_acquire)) {
        _quit.store(false, std::memory_order_release);
        if(getProperties().contains("Priority")) {
            _priority.store(Ichor::any_cast<uint64_t>(getProperties()["Priority"]), std::memory_order_release);
        }

        if(!getProperties().contains("Port") || !getProperties().contains("Address")) {
            ICHOR_LOG_ERROR_ATOMIC(_logger, "Missing port or address when starting HttpConnectionService");
            co_return tl::unexpected(StartError::FAILED);
        }

        if(getProperties().contains("NoDelay")) {
            _tcpNoDelay.store(Ichor::any_cast<bool>(getProperties()["NoDelay"]), std::memory_order_release);
        }
//        fmt::print("connecting to {}\n", Ichor::any_cast<std::string&>(getProperties()["Address"]));

        boost::system::error_code ec;
        auto address = net::ip::make_address(Ichor::any_cast<std::string &>(getProperties()["Address"]), ec);
        auto port = Ichor::any_cast<uint16_t>(getProperties()["Port"]);

        if(ec) {
            ICHOR_LOG_ERROR_ATOMIC(_logger, "Couldn't parse address \"{}\": {} {}", Ichor::any_cast<std::string &>(getProperties()["Address"]), ec.value(), ec.message());
            co_return tl::unexpected(StartError::FAILED);
        }

        if (getProperties().contains("ConnectOverSsl")) {
            _useSsl.store(Ichor::any_cast<bool>(getProperties()["ConnectOverSsl"]), std::memory_order_release);
        }

        _connecting.store(true, std::memory_order_release);
        net::spawn(*_asioContextService->getContext(), [this, address = std::move(address), port](net::yield_context yield) {
            connect(tcp::endpoint{address, port}, std::move(yield));
        }ASIO_SPAWN_COMPLETION_TOKEN);
    }

    if (_asioContextService->fibersShouldStop()) {
        co_return tl::unexpected(StartError::FAILED);
    }

    co_await _startStopEvent;
    _startStopEvent.reset();

    co_return {};
}

Ichor::Task<void> Ichor::HttpConnectionService::stop() {
    _quit.store(true, std::memory_order_release);
//    INTERNAL_DEBUG("----------------------------------------------- STOP");

    co_await close();

//    INTERNAL_DEBUG("----------------------------------------------- STOP DONE");

    co_return;
}

void Ichor::HttpConnectionService::addDependencyInstance(ILogger &logger, IService &) {
    _logger.store(&logger, std::memory_order_release);
}

void Ichor::HttpConnectionService::removeDependencyInstance(ILogger &logger, IService&) {
    _logger.store(nullptr, std::memory_order_release);
}

void Ichor::HttpConnectionService::addDependencyInstance(IAsioContextService &AsioContextService, IService&) {
    _asioContextService = &AsioContextService;
    ICHOR_LOG_TRACE_ATOMIC(_logger, "Inserted AsioContextService");
}

void Ichor::HttpConnectionService::removeDependencyInstance(IAsioContextService&, IService&) {
    _asioContextService = nullptr;
}

void Ichor::HttpConnectionService::setPriority(uint64_t priority) {
    _priority.store(priority, std::memory_order_release);
}

uint64_t Ichor::HttpConnectionService::getPriority() {
    return _priority.load(std::memory_order_acquire);
}

Ichor::Task<Ichor::HttpResponse> Ichor::HttpConnectionService::sendAsync(Ichor::HttpMethod method, std::string_view route, unordered_map<std::string, std::string> &&headers, std::vector<uint8_t> &&msg) {
    if(method == HttpMethod::get && !msg.empty()) {
        throw std::runtime_error("GET requests cannot have a body.");
    }

    ICHOR_LOG_DEBUG_ATOMIC(_logger, "sending to {}", route);

    HttpResponse response{};

    if(_quit.load(std::memory_order_acquire) || _asioContextService->fibersShouldStop()) {
        co_return response;
    }

    AsyncManualResetEvent event{};

    net::spawn(*_asioContextService->getContext(), [this, method, route, &event, &response, &headers, &msg](net::yield_context yield) mutable {
        static_assert(std::is_trivially_copyable_v<Detail::ConnectionOutboxMessage>, "ConnectionOutboxMessage should be trivially copyable");
        ScopeGuardAtomicCount const guard{_finishedListenAndRead};

        std::unique_lock lg{_outboxMutex};
        if(_outbox.full()) {
            _outbox.set_capacity(std::max<uint64_t>(_outbox.capacity() * 2, 10ul));
        }
        _outbox.push_back({method, route, &event, &response, &headers, &msg});
        if(_outbox.size() > 1) {
            // handled by existing net::spawn
            return;
        }
        while(!_outbox.empty()) {
            // Copy message, should be trivially copyable and prevents iterator invalidation
            auto next = _outbox.front();
            lg.unlock();
            INTERNAL_DEBUG("Outbox {}", next.route);

            ScopeGuard const coroutineGuard{[this, event = next.event, &lg]() {
                // use service id 0 to ensure event gets run, even if service is stopped. Otherwise, the coroutine will never complete.
                // Similarly, use priority 0 to ensure these events run before any dependency changes, otherwise the service might be destroyed
                // before we can finish all the coroutines.
                _queue->pushPrioritisedEvent<RunFunctionEvent>(0u, 0u, [event]() {
                    event->set();
                });
                lg.lock();
                _outbox.pop_front();
            }};

            // if the service has to quit, we still have to spool through all the remaining messages, to complete coroutines
            if(_quit.load(std::memory_order_acquire)) {
                continue;
            }

            beast::error_code ec;
            http::request<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>> req{
                static_cast<http::verb>(next.method),
                next.route,
                11,
                std::move(*next.body)
            };

            for (auto const &header : *next.headers) {
                req.set(header.first, header.second);
            }
            req.set(http::field::host, Ichor::any_cast<std::string &>(getProperties()["Address"]));
            req.prepare_payload();
            req.keep_alive();

            if(_useSsl.load(std::memory_order_acquire)) {
                // Set the timeout for this operation.
                // _sslStream should only be modified from the boost thread
                beast::get_lowest_layer(*_sslStream).expires_after(30s);
                INTERNAL_DEBUG("https::write");
                http::async_write(*_sslStream, req, yield[ec]);
            } else {
                // Set the timeout for this operation.
                // _httpStream should only be modified from the boost thread
                _httpStream->expires_after(30s);
                INTERNAL_DEBUG("http::write");
                http::async_write(*_httpStream, req, yield[ec]);
                INTERNAL_DEBUG("http::write done");
            }
            if (ec) {
                fail(ec, "HttpConnectionService::sendAsync write");
                continue;
            }

            // if the service has to quit, we still have to spool through all the remaining messages, to complete coroutines
            if(_quit.load(std::memory_order_acquire)) {
                continue;
            }

            // This buffer is used for reading and must be persisted
            beast::basic_flat_buffer b{std::allocator<uint8_t>{}};

            // Declare a container to hold the response
            http::response<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>> res;

            // Receive the HTTP response
            if(_useSsl.load(std::memory_order_acquire)) {
                INTERNAL_DEBUG("https::read");
                http::async_read(*_sslStream, b, res, yield[ec]);
            } else {
                INTERNAL_DEBUG("http::read");
                http::async_read(*_httpStream, b, res, yield[ec]);
                INTERNAL_DEBUG("http::read done");
            }
            if (ec) {
                fail(ec, "HttpConnectionService::sendAsync read");
                continue;
            }
            // rapidjson f.e. expects a null terminator
            if(!res.body().empty() && *res.body().rbegin() != 0) {
                res.body().push_back(0);
            }

            INTERNAL_DEBUG("received HTTP response {}", std::string_view(reinterpret_cast<char *>(res.body().data()), res.body().size()));
            // unset the timeout for the next operation.
            if(_useSsl.load(std::memory_order_acquire)) {
                beast::get_lowest_layer(*_sslStream).expires_never();
            } else {
                _httpStream->expires_never();
            }

            next.response->status = (HttpStatus) (int) res.result();
            next.response->headers.reserve(static_cast<unsigned long>(std::distance(std::begin(res), std::end(res))));
            for (auto const &header: res) {
                next.response->headers.emplace(header.name_string(), header.value());
            }

            // need to use move iterator instead of std::move directly, to prevent leaks.
            next.response->body.insert(next.response->body.end(), std::make_move_iterator(res.body().begin()), std::make_move_iterator(res.body().end()));
        }
    }ASIO_SPAWN_COMPLETION_TOKEN);

    co_await event;

    co_return response;
}

Ichor::Task<void> Ichor::HttpConnectionService::close() {
    if(_useSsl.load(std::memory_order_acquire)) {
        if(!_sslStream) {
            co_return;
        }
    } else {
        if(!_httpStream) {
            co_return;
        }
    }

    if(!_connecting.exchange(true, std::memory_order_acq_rel)) {
        net::spawn(*_asioContextService->getContext(), [this](net::yield_context) {
            // _httpStream and _sslStream should only be modified from the boost thread
            if(_useSsl.load(std::memory_order_acquire)) {
                beast::get_lowest_layer(*_sslStream).cancel();
            } else {
                _httpStream->cancel();
            }

            _queue->pushEvent<RunFunctionEvent>(getServiceId(), [this]() {
                _startStopEvent.set();
            });
        }ASIO_SPAWN_COMPLETION_TOKEN);
    }

    co_await _startStopEvent;
    _startStopEvent.reset();

    while(_finishedListenAndRead.load(std::memory_order_acquire) != 0) {
        std::this_thread::sleep_for(1ms);
    }

    _connected.store(false, std::memory_order_release);
    _connecting.store(false, std::memory_order_release);
    _httpStream = nullptr;
    _sslStream = nullptr;

    co_return;
}

void Ichor::HttpConnectionService::fail(beast::error_code ec, const char *what) {
    ICHOR_LOG_ERROR_ATOMIC(_logger, "Boost.BEAST fail: {}, {}", what, ec.message());
    _queue->pushPrioritisedEvent<StopServiceEvent>(getServiceId(), _priority.load(std::memory_order_acquire), getServiceId());
}

void Ichor::HttpConnectionService::connect(tcp::endpoint endpoint, net::yield_context yield) {
    ScopeGuardAtomicCount guard{_finishedListenAndRead};
    beast::error_code ec;

    tcp::resolver resolver(*_asioContextService->getContext());

    auto const results = resolver.resolve(endpoint, ec);
    if(ec) {
        return fail(ec, "HttpConnectionService::connect resolve");
    }

    if (_useSsl.load(std::memory_order_acquire)) {
        // _sslContext and _sslStream should only be modified from the boost thread
        _sslContext = std::make_unique<net::ssl::context>(net::ssl::context::tlsv12);
        _sslContext->set_verify_mode(net::ssl::verify_peer);

        if(getProperties().contains("RootCA")) {
            std::string &ca = Ichor::any_cast<std::string&>(getProperties()["RootCA"]);
            _sslContext->add_certificate_authority(boost::asio::const_buffer(ca.c_str(), ca.size()), ec);
        }

        _sslStream = std::make_unique<beast::ssl_stream<beast::tcp_stream>>(*_asioContextService->getContext(), *_sslContext);

        if(!SSL_set_tlsext_host_name(_sslStream->native_handle(), endpoint.address().to_string().c_str())) {
            ec.assign(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category());
            return fail(ec, "HttpConnectionService::connect ssl set host name");
        }

        // Never expire until we actually have an operation.
        beast::get_lowest_layer(*_sslStream).expires_never();

        // Make the connection on the IP address we get from a lookup
        int attempts{};
        while(!_quit.load(std::memory_order_acquire) && !_asioContextService->fibersShouldStop() && attempts < 5) {
            beast::get_lowest_layer(*_sslStream).async_connect(results, yield[ec]);
            if (ec) {
                attempts++;
                net::steady_timer t{*_asioContextService->getContext()};
                t.expires_after(250ms);
                t.async_wait(yield);
            } else {
                break;
            }
        }

        fmt::print("--- ssl tcpNoDelay: {} ---\n", _tcpNoDelay.load(std::memory_order_acquire));
        beast::get_lowest_layer(*_sslStream).socket().set_option(tcp::no_delay(_tcpNoDelay.load(std::memory_order_acquire)));

        _sslStream->async_handshake(net::ssl::stream_base::client, yield[ec]);
        if(ec) {
            return fail(ec, "HttpConnectionService::connect ssl handshake");
        }
    } else {
        // _httpStream should only be modified from the boost thread
        _httpStream = std::make_unique<beast::tcp_stream>(*_asioContextService->getContext());

        // Never expire until we actually have an operation.
        _httpStream->expires_never();

        // Make the connection on the IP address we get from a lookup
        int attempts{};
        while(!_quit.load(std::memory_order_acquire) && !_asioContextService->fibersShouldStop() && attempts < 5) {
            _httpStream->async_connect(results, yield[ec]);
            if (ec) {
                attempts++;
                net::steady_timer t{*_asioContextService->getContext()};
                t.expires_after(250ms);
                t.async_wait(yield);
            } else {
                break;
            }
        }

        _httpStream->socket().set_option(tcp::no_delay(_tcpNoDelay.load(std::memory_order_acquire)));
    }

    if(ec) {
        // see below for why _connecting has to be set last
        _quit.store(true, std::memory_order_release);
        _connecting.store(false, std::memory_order_release);
        return fail(ec, "HttpConnectionService::connect connect");
    }

    // set connected before connecting, or races with the start() function may occur.
    _queue->pushEvent<RunFunctionEvent>(getServiceId(), [this]() {
        _startStopEvent.set();
    });
    _connected.store(true, std::memory_order_release);
    _connecting.store(false, std::memory_order_release);
}
