#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/DependencyManager.h>
#include <ichor/services/network/http/HttpHostService.h>
#include <ichor/services/network/http/HttpScopeGuardFinish.h>

Ichor::HttpHostService::HttpHostService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
    reg.registerDependency<ILogger>(this, true);
    reg.registerDependency<IHttpContextService>(this, true);
}

Ichor::StartBehaviour Ichor::HttpHostService::start() {
    if(getProperties().contains("Priority")) {
        _priority = Ichor::any_cast<uint64_t>(getProperties().operator[]("Priority"));
    }

    if(!getProperties().contains("Port") || !getProperties().contains("Address")) {
        getManager().pushPrioritisedEvent<UnrecoverableErrorEvent>(getServiceId(), _priority, 0, "Missing port or address when starting HttpHostService");
        return Ichor::StartBehaviour::FAILED_DO_NOT_RETRY;
    }

    if(getProperties().contains("NoDelay")) {
        _tcpNoDelay = Ichor::any_cast<bool>(getProperties().operator[]("NoDelay"));
    }

    auto address = net::ip::make_address(Ichor::any_cast<std::string&>(getProperties().operator[]("Address")));
    auto port = Ichor::any_cast<uint16_t>(getProperties().operator[]("Port"));

    net::spawn(*_httpContextService->getContext(), [this, address = std::move(address), port](net::yield_context yield){
        listen(tcp::endpoint{address, port}, std::move(yield));
    });

    return Ichor::StartBehaviour::SUCCEEDED;
}

Ichor::StartBehaviour Ichor::HttpHostService::stop() {
    INTERNAL_DEBUG("HttpHostService::stop()");
    _quit = true;

    if(!_goingToCleanupStream.exchange(true) && _httpAcceptor) {
        if (_httpAcceptor->is_open()) {
            _httpAcceptor->close();
        }
        for(auto &[id, stream] : _httpStreams) {
            stream->cancel();
        }

        _httpAcceptor = nullptr;
        _cleanedupStream = true;
    }

    if(_finishedListenAndRead.load(std::memory_order_acquire) != 0 || !_cleanedupStream) {
        return Ichor::StartBehaviour::FAILED_AND_RETRY;
    }
    _httpStreams.clear();

    return Ichor::StartBehaviour::SUCCEEDED;
}

void Ichor::HttpHostService::addDependencyInstance(ILogger *logger, IService *) {
    _logger = logger;
}

void Ichor::HttpHostService::removeDependencyInstance(ILogger *logger, IService *) {
    _logger = nullptr;
}

void Ichor::HttpHostService::addDependencyInstance(IHttpContextService *httpContextService, IService *) {
    _httpContextService = httpContextService;
    ICHOR_LOG_TRACE(_logger, "Inserted httpContextService");
}

void Ichor::HttpHostService::removeDependencyInstance(IHttpContextService *httpContextService, IService *) {
    ICHOR_LOG_TRACE(_logger, "Removing httpContextService");
    stop();
    while(_finishedListenAndRead.load(std::memory_order_acquire) != 0) {
        // sleep this thread so that this thread won´t starve other threads (especially noticeable under callgrind)
        std::this_thread::sleep_for(1ms);
    }
    _httpContextService = nullptr;
}

void Ichor::HttpHostService::setPriority(uint64_t priority) {
    _priority.store(priority, std::memory_order_release);
}

uint64_t Ichor::HttpHostService::getPriority() {
    return _priority.load(std::memory_order_acquire);
}

std::unique_ptr<Ichor::HttpRouteRegistration> Ichor::HttpHostService::addRoute(HttpMethod method, std::string_view route, std::function<AsyncGenerator<HttpResponse>(HttpRequest&)> handler) {
    auto &routes = _handlers[method];

    auto existingHandler = routes.find(route);

    if(existingHandler != std::end(routes)) {
        throw std::runtime_error("Route already present in handlers");
    }

    routes.emplace(route, handler);

    return std::make_unique<HttpRouteRegistration>(method, route, this);
}

void Ichor::HttpHostService::removeRoute(HttpMethod method, std::string_view route) {
    auto routes = _handlers.find(method);

    if(routes == std::end(_handlers)) {
        return;
    }

    auto it = routes->second.find(route);

    if(it != std::end(routes->second)) {
        routes->second.erase(it);
    }

}

void Ichor::HttpHostService::fail(beast::error_code ec, const char *what, bool stopSelf) {
    ICHOR_LOG_ERROR(_logger, "Boost.BEAST fail: {}, {}", what, ec.message());
    if(stopSelf) {
        getManager().pushPrioritisedEvent<StopServiceEvent>(getServiceId(), _priority.load(std::memory_order_acquire), getServiceId());
    }
}

void Ichor::HttpHostService::listen(tcp::endpoint endpoint, net::yield_context yield)
{
    ScopeGuardAtomicCount guard{_finishedListenAndRead};
    beast::error_code ec;

    _httpAcceptor = std::make_unique<tcp::acceptor>(*_httpContextService->getContext());
    _httpAcceptor->open(endpoint.protocol(), ec);
    if(ec) {
        return fail(ec, "HttpHostService::listen open", true);
    }

    _httpAcceptor->set_option(net::socket_base::reuse_address(true), ec);
    if(ec) {
        return fail(ec, "HttpHostService::listen set_option", true);
    }

    _httpAcceptor->bind(endpoint, ec);
    if(ec) {
        return fail(ec, "HttpHostService::listen bind", true);
    }

    _httpAcceptor->listen(net::socket_base::max_listen_connections, ec);
    if(ec) {
        return fail(ec, "HttpHostService::listen listen", true);
    }

    while(!_quit && !_httpContextService->fibersShouldStop())
    {
        auto socket = tcp::socket(*_httpContextService->getContext());

        // tcp accept new connections
        _httpAcceptor->async_accept(socket, yield[ec]);
        if(ec)
        {
            fail(ec, "HttpHostService::listen accept", false);
            continue;
        }

        socket.set_option(tcp::no_delay(_tcpNoDelay));

        net::spawn(*_httpContextService->getContext(), [this, socket = std::move(socket)](net::yield_context _yield) mutable {
            if(_quit) {
                return;
            }

            read(std::move(socket), std::move(_yield));
        });
    }

    ICHOR_LOG_WARN(_logger, "finished listen() {} {}", _quit, _httpContextService->fibersShouldStop());
    stop();
}

void Ichor::HttpHostService::read(tcp::socket socket, net::yield_context yield) {
    ScopeGuardAtomicCount guard{_finishedListenAndRead};
    beast::error_code ec;
    auto addr = socket.remote_endpoint().address().to_string();
    uint64_t streamId = _streamIdCounter++;
    auto *httpStream = _httpStreams.emplace(streamId, std::make_unique<beast::tcp_stream>(std::move(socket))).first->second.get();

    // This buffer is required to persist across reads
    beast::basic_flat_buffer buffer{std::allocator<uint8_t>{}};

    while(!_quit && !_httpContextService->fibersShouldStop())
    {
        // Set the timeout.
        httpStream->expires_after(30s);

        // Read a request
        http::request<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>> req;
        http::async_read(*httpStream, buffer, req, yield[ec]);
        if(ec == http::error::end_of_stream) {
            fail(ec, "HttpHostService::read end of stream", false);
            break;
        }
        if(ec == net::error::operation_aborted) {
            fail(ec, "HttpHostService::read operation aborted", false);
            break;
        }
        if(ec == net::error::timed_out) {
            fail(ec, "HttpHostService::read operation timed out", false);
            break;
        }
        if(ec == net::error::bad_descriptor) {
            fail(ec, "HttpHostService::read bad descriptor", false);
            break;
        }
        if(ec) {
            fail(ec, "HttpHostService::read read", false);
            continue;
        }

        ICHOR_LOG_TRACE(_logger, "New request for {} {}", (int) req.method(), req.target());

        std::vector<HttpHeader> headers{};
        headers.reserve(std::distance(std::begin(req), std::end(req)));
        for (auto const &field: req) {
            headers.emplace_back(field.name_string(), field.value());
        }
        HttpRequest httpReq{std::move(req.body()), static_cast<HttpMethod>(req.method()), std::string{req.target()}, addr, std::move(headers)};

        getManager().pushEvent<RunFunctionEvent>(getServiceId(), [this, streamId, httpReq = std::move(httpReq), version = req.version(), keep_alive = req.keep_alive()](DependencyManager &dm) mutable -> AsyncGenerator<void> {

            auto routes = _handlers.find(static_cast<HttpMethod>(httpReq.method));

            if (routes != std::end(_handlers)) {
                auto handler = routes->second.find(httpReq.route);

                if (handler != std::end(routes->second)) {

                    // using reference here leads to heap use after free. Not sure why.
                    HttpResponse httpRes = std::move(*co_await handler->second(httpReq).begin());
                    http::response<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>> res{static_cast<http::status>(httpRes.status),
                                                                                                                version};
                    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                    res.set(http::field::content_type, "text/html");
                    for (auto const &header: httpRes.headers) {
                        res.set(header.value, header.name);
                    }
                    res.keep_alive(keep_alive);
                    ICHOR_LOG_TRACE(_logger, "sending http response {} - {}", (int) httpRes.status,
                                    std::string_view(reinterpret_cast<char *>(httpRes.body.data()), httpRes.body.size()));

                    res.body() = std::move(httpRes.body);
                    res.prepare_payload();
                    sendInternal(streamId, std::move(res));

                    co_return;
                }
            }

            http::response<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>> res{http::status::not_found, version};
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/html");
            res.keep_alive(keep_alive);
            res.prepare_payload();
            sendInternal(streamId, std::move(res));

            co_return;
        });
    }
    _httpStreams.erase(streamId);

    // At this point the connection is closed gracefully
    ICHOR_LOG_WARN(_logger, "finished read() {} {}", _quit, _httpContextService->fibersShouldStop());
}

void Ichor::HttpHostService::sendInternal(uint64_t streamId, http::response<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>> &&res) {
    static_assert(std::is_move_assignable_v<Detail::HostOutboxMessage>, "HostOutboxMessage should be move assignable");

    net::spawn(*_httpContextService->getContext(), [this, streamId, res = std::move(res)](net::yield_context yield) mutable {
        if(_quit) {
            return;
        }

        if(_outbox.full()) {
            _outbox.set_capacity(std::max(_outbox.capacity() * 2, 10ul));
        }
        _outbox.push_back({streamId, std::move(res)});
        if(_outbox.size() > 1) {
            // handled by existing net::spawn
            return;
        }

        while(!_outbox.empty()) {
            // Move message, should be trivially copyable and prevents iterator invalidation
            auto next = std::move(_outbox.front());
            auto streamIt = _httpStreams.find(next.streamId);

            if (streamIt == end(_httpStreams)) {
                ICHOR_LOG_WARN(_logger, "http stream id {} already disconnected, cannot send response {} {} {}", streamId, _httpStreams.size(), _outbox.size(), _outbox.capacity());
                _outbox.pop_front();
                continue;
            }

            streamIt->second->expires_after(30s);
            beast::error_code ec;
            http::async_write(*streamIt->second, next.res, yield[ec]);
            if (ec == http::error::end_of_stream) {
                fail(ec, "HttpHostService::sendInternal end of stream", false);
            } else if (ec == net::error::operation_aborted) {
                fail(ec, "HttpHostService::sendInternal operation aborted", false);
            } else if (ec == net::error::bad_descriptor) {
                fail(ec, "HttpHostService::sendInternal bad descriptor", false);
            } else if (ec) {
                fail(ec, "HttpHostService::sendInternal write", false);
            }
            _outbox.pop_front();
        }
    });
}

#endif