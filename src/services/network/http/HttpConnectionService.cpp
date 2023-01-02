#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/DependencyManager.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/services/network/http/HttpConnectionService.h>
#include <ichor/services/network/http/HttpScopeGuards.h>
#include <ichor/services/network/NetworkEvents.h>

Ichor::HttpConnectionService::HttpConnectionService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
    reg.registerDependency<ILogger>(this, true);
    reg.registerDependency<IAsioContextService>(this, true);
}

Ichor::AsyncGenerator<tl::expected<void, Ichor::StartError>> Ichor::HttpConnectionService::start() {
    if(!_asioContextService->fibersShouldStop() && !_connecting.load(std::memory_order_acquire) && !_connected.load(std::memory_order_acquire)) {
        _quit.store(false, std::memory_order_release);
        if (getProperties().contains("Priority")) {
            _priority.store(Ichor::any_cast<uint64_t>(getProperties()["Priority"]), std::memory_order_release);
        }

        if (!getProperties().contains("Port") || !getProperties().contains("Address")) {
            ICHOR_LOG_ERROR_ATOMIC(_logger, "Missing port or address when starting HttpConnectionService");
            co_return tl::unexpected(StartError::FAILED);
        }

        if(getProperties().contains("NoDelay")) {
            _tcpNoDelay.store(Ichor::any_cast<bool>(getProperties()["NoDelay"]), std::memory_order_release);
        }
//        fmt::print("connecting to {}\n", Ichor::any_cast<std::string&>(getProperties()["Address"]));

        auto address = net::ip::make_address(Ichor::any_cast<std::string &>(getProperties()["Address"]));
        auto port = Ichor::any_cast<uint16_t>(getProperties()["Port"]);

        _connecting.store(true, std::memory_order_release);
        net::spawn(*_asioContextService->getContext(), [this, address = std::move(address), port](net::yield_context yield) {
            connect(tcp::endpoint{address, port}, std::move(yield));
        }ASIO_SPAWN_COMPLETION_TOKEN);
    }

    if(_asioContextService->fibersShouldStop()) {
        co_return tl::unexpected(StartError::FAILED);
    }

    co_await _startStopEvent;
    _startStopEvent.reset();

    co_return {};
}

Ichor::AsyncGenerator<void> Ichor::HttpConnectionService::stop() {
    _quit.store(true, std::memory_order_release);
//    INTERNAL_DEBUG("----------------------------------------------- STOP");

    co_await close().begin();

//    INTERNAL_DEBUG("----------------------------------------------- STOP DONE");

    co_return;
}

void Ichor::HttpConnectionService::addDependencyInstance(ILogger *logger, IService *) {
    _logger.store(logger, std::memory_order_release);
}

void Ichor::HttpConnectionService::removeDependencyInstance(ILogger *logger, IService *) {
    _logger.store(nullptr, std::memory_order_release);
}

void Ichor::HttpConnectionService::addDependencyInstance(IAsioContextService *AsioContextService, IService *) {
    _asioContextService = AsioContextService;
    ICHOR_LOG_TRACE_ATOMIC(_logger, "Inserted AsioContextService");
}

void Ichor::HttpConnectionService::removeDependencyInstance(IAsioContextService *AsioContextService, IService *) {
    _asioContextService = nullptr;
}

void Ichor::HttpConnectionService::setPriority(uint64_t priority) {
    _priority.store(priority, std::memory_order_release);
}

uint64_t Ichor::HttpConnectionService::getPriority() {
    return _priority.load(std::memory_order_acquire);
}

Ichor::AsyncGenerator<Ichor::HttpResponse> Ichor::HttpConnectionService::sendAsync(Ichor::HttpMethod method, std::string_view route, std::vector<HttpHeader> &&headers, std::vector<uint8_t> &&msg) {
    if(method == HttpMethod::get && !msg.empty()) {
        throw std::runtime_error("GET requests cannot have a body.");
    }

    ICHOR_LOG_DEBUG_ATOMIC(_logger, "sending to {}", route);

    HttpResponse response{};
    response.error = true;

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

            ScopeGuardFunction const coroutineGuard{[this, &event, &lg]() {
                // use service id 0 to ensure event gets run, even if service is stopped. Otherwise, the coroutine will never complete.
                // Similarly, use priority 0 to ensure these events run before any dependency changes, otherwise the service might be destroyed
                // before we can finish all the coroutines.
                getManager().pushPrioritisedEvent<RunFunctionEvent>(0u, 0u, [&event](DependencyManager &) {
                    event.set();
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
                req.set(header.name, header.value);
            }
            req.set(http::field::host, Ichor::any_cast<std::string &>(getProperties()["Address"]));
            req.prepare_payload();
            req.keep_alive();

            // Set the timeout for this operation.
            // _httpStream should only be modified from the boost thread
            _httpStream->expires_after(30s);
            INTERNAL_DEBUG("http::write");
            http::async_write(*_httpStream, req, yield[ec]);
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

            INTERNAL_DEBUG("http::read");
            // Receive the HTTP response
            http::async_read(*_httpStream, b, res, yield[ec]);
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
            _httpStream->expires_never();

            next.response->status = (HttpStatus) (int) res.result();
            next.response->headers.reserve(static_cast<unsigned long>(std::distance(std::begin(res), std::end(res))));
            for (auto const &header: res) {
                next.response->headers.emplace_back(header.name_string(), header.value());
            }

            // need to use move iterator instead of std::move directly, to prevent leaks.
            next.response->body.insert(next.response->body.end(), std::make_move_iterator(res.body().begin()), std::make_move_iterator(res.body().end()));
        }
    }ASIO_SPAWN_COMPLETION_TOKEN);

    co_await event;

    co_return response;
}

Ichor::AsyncGenerator<void> Ichor::HttpConnectionService::close() {
    if(!_httpStream) {
        co_return;
    }

    if(!_connecting.exchange(true, std::memory_order_acq_rel)) {
        net::spawn(*_asioContextService->getContext(), [this](net::yield_context) {
            // _httpStream should only be modified from the boost thread
            _httpStream->cancel();

            getManager().pushEvent<RunFunctionEvent>(getServiceId(), [this](DependencyManager &) {
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

    co_return;
}

void Ichor::HttpConnectionService::fail(beast::error_code ec, const char *what) {
    ICHOR_LOG_ERROR_ATOMIC(_logger, "Boost.BEAST fail: {}, {}", what, ec.message());
    getManager().pushPrioritisedEvent<StopServiceEvent>(getServiceId(), _priority.load(std::memory_order_acquire), getServiceId());
}

void Ichor::HttpConnectionService::connect(tcp::endpoint endpoint, net::yield_context yield) {
    ScopeGuardAtomicCount guard{_finishedListenAndRead};
    beast::error_code ec;

    tcp::resolver resolver(*_asioContextService->getContext());
    // _httpStream should only be modified from the boost thread
    _httpStream = std::make_unique<beast::tcp_stream>(*_asioContextService->getContext());

    auto const results = resolver.resolve(endpoint, ec);
    if(ec) {
        return fail(ec, "HttpConnectionService::connect resolve");
    }

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

    _httpStream->socket().set_option(tcp::no_delay(_tcpNoDelay));

    if(ec) {
        // see below for why _connecting has to be set last
        _quit.store(true, std::memory_order_release);
        _connecting.store(false, std::memory_order_release);
        return fail(ec, "HttpConnectionService::connect connect");
    }

    // set connected before connecting, or races with the start() function may occur.
    getManager().pushEvent<RunFunctionEvent>(getServiceId(), [this](DependencyManager &dm) {
        _startStopEvent.set();
    });
    _connected.store(true, std::memory_order_release);
    _connecting.store(false, std::memory_order_release);
}

#endif
