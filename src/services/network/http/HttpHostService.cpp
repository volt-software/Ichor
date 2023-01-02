#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/DependencyManager.h>
#include <ichor/services/network/http/HttpHostService.h>
#include <ichor/services/network/http/HttpScopeGuards.h>

Ichor::HttpHostService::HttpHostService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
    reg.registerDependency<ILogger>(this, true);
    reg.registerDependency<IAsioContextService>(this, true);
}

Ichor::AsyncGenerator<tl::expected<void, Ichor::StartError>> Ichor::HttpHostService::start() {
    if(getProperties().contains("Priority")) {
        _priority = Ichor::any_cast<uint64_t>(getProperties()["Priority"]);
    }

    if(getProperties().contains("SendServerHeader")) {
        _sendServerHeader = Ichor::any_cast<bool>(getProperties()["SendServerHeader"]);
    }

    if(!getProperties().contains("Port") || !getProperties().contains("Address")) {
        ICHOR_LOG_ERROR_ATOMIC(_logger, "Missing port or address when starting HttpHostService");
        co_return tl::unexpected(StartError::FAILED);
    }

    if(getProperties().contains("NoDelay")) {
        _tcpNoDelay.store(Ichor::any_cast<bool>(getProperties()["NoDelay"]), std::memory_order_release);
    }

    auto address = net::ip::make_address(Ichor::any_cast<std::string&>(getProperties()["Address"]));
    auto port = Ichor::any_cast<uint16_t>(getProperties()["Port"]);

    net::spawn(*_asioContextService->getContext(), [this, address = std::move(address), port](net::yield_context yield){
        listen(tcp::endpoint{address, port}, std::move(yield));
    }ASIO_SPAWN_COMPLETION_TOKEN);

    co_return {};
}

Ichor::AsyncGenerator<void> Ichor::HttpHostService::stop() {
    INTERNAL_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! HttpHostService::stop()");
    _quit.store(true, std::memory_order_release);

    if(!_goingToCleanupStream.exchange(true, std::memory_order_acq_rel) && _httpAcceptor) {
        if (_httpAcceptor->is_open()) {
            _httpAcceptor->close();
        }
        net::spawn(*_asioContextService->getContext(), [this](net::yield_context _yield) {
            ScopeGuardAtomicCount guard{_finishedListenAndRead};
            {
                std::unique_lock lg{_streamsMutex};
                for (auto &[id, stream]: _httpStreams) {
//                stream->quit.store(true, std::memory_order_release);
                    stream->socket.cancel();
                }
            }

            _httpAcceptor = nullptr;

            getManager().pushEvent<RunFunctionEvent>(getServiceId(), [this](DependencyManager &dm) {
                _startStopEvent.set();
            });
        }ASIO_SPAWN_COMPLETION_TOKEN);
    }

    co_await _startStopEvent;
    INTERNAL_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! post await");

    while(_finishedListenAndRead.load(std::memory_order_acquire) != 0) {
        std::this_thread::sleep_for(1ms);
    }

    _httpStreams.clear();
    INTERNAL_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! done");

    co_return;
}

void Ichor::HttpHostService::addDependencyInstance(ILogger *logger, IService *) {
    _logger.store(logger, std::memory_order_release);
}

void Ichor::HttpHostService::removeDependencyInstance(ILogger *logger, IService *) {
    _logger.store(nullptr, std::memory_order_release);
}

void Ichor::HttpHostService::addDependencyInstance(IAsioContextService *AsioContextService, IService *) {
    _asioContextService = AsioContextService;
    ICHOR_LOG_TRACE_ATOMIC(_logger, "Inserted AsioContextService");
}

void Ichor::HttpHostService::removeDependencyInstance(IAsioContextService *AsioContextService, IService *) {
    ICHOR_LOG_TRACE_ATOMIC(_logger, "Removing AsioContextService");
    _asioContextService = nullptr;
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
    ICHOR_LOG_ERROR_ATOMIC(_logger, "Boost.BEAST fail: {}, {}", what, ec.message());
    if(stopSelf) {
        getManager().pushPrioritisedEvent<StopServiceEvent>(getServiceId(), _priority.load(std::memory_order_acquire), getServiceId());
    }
}

void Ichor::HttpHostService::listen(tcp::endpoint endpoint, net::yield_context yield) {
    ScopeGuardAtomicCount guard{_finishedListenAndRead};
    beast::error_code ec;

    _httpAcceptor = std::make_unique<tcp::acceptor>(*_asioContextService->getContext());
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

    while(!_quit.load(std::memory_order_acquire) && !_asioContextService->fibersShouldStop())
    {
        auto socket = tcp::socket(*_asioContextService->getContext());

        // tcp accept new connections
        _httpAcceptor->async_accept(socket, yield[ec]);
        if(ec)
        {
            fail(ec, "HttpHostService::listen accept", false);
            continue;
        }

        socket.set_option(tcp::no_delay(_tcpNoDelay.load(std::memory_order_acquire)));

        net::spawn(*_asioContextService->getContext(), [this, socket = std::move(socket)](net::yield_context _yield) mutable {
            if(_quit.load(std::memory_order_acquire)) {
                return;
            }

            read(std::move(socket), std::move(_yield));
        }ASIO_SPAWN_COMPLETION_TOKEN);
    }

    ICHOR_LOG_WARN_ATOMIC(_logger, "finished listen() {} {}", _quit.load(std::memory_order_acquire), _asioContextService->fibersShouldStop());
    if(!_goingToCleanupStream.exchange(true, std::memory_order_acq_rel) && _httpAcceptor) {
        getManager().pushPrioritisedEvent<StopServiceEvent>(getServiceId(), getPriority(), getServiceId());
    }
}

void Ichor::HttpHostService::read(tcp::socket socket, net::yield_context yield) {
    ScopeGuardAtomicCount guard{_finishedListenAndRead};
    beast::error_code ec;
    auto addr = socket.remote_endpoint().address().to_string();
    std::shared_ptr<Detail::Connection> connection;
    uint64_t streamId;
    {
        std::lock_guard lg(_streamsMutex);
        streamId = _streamIdCounter++;
        connection = _httpStreams.emplace(streamId, std::make_shared<Detail::Connection>(std::move(socket))).first->second;
    }

    // This buffer is required to persist across reads
    beast::basic_flat_buffer buffer{std::allocator<uint8_t>{}};

    while(!_quit.load(std::memory_order_acquire) && !_asioContextService->fibersShouldStop())
    {
        // Set the timeout.
        connection->socket.expires_after(30s);

        // Read a request
        http::request<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>> req;
        http::async_read(connection->socket, buffer, req, yield[ec]);
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

        ICHOR_LOG_TRACE_ATOMIC(_logger, "New request for {} {}", (int) req.method(), req.target());

        std::vector<HttpHeader> headers{};
        headers.reserve(static_cast<unsigned long>(std::distance(std::begin(req), std::end(req))));
        for (auto const &field : req) {
            headers.emplace_back(field.name_string(), field.value());
        }
        // rapidjson f.e. expects a null terminator
        if(!req.body().empty() && *req.body().rbegin() != 0) {
            req.body().push_back(0);
        }
        HttpRequest httpReq{std::move(req.body()), static_cast<HttpMethod>(req.method()), std::string{req.target()}, addr, std::move(headers)};

        getManager().pushEvent<RunFunctionEventAsync>(getServiceId(), [this, connection, httpReq = std::move(httpReq), version = req.version(), keep_alive = req.keep_alive()](DependencyManager &dm) mutable -> AsyncGenerator<IchorBehaviour> {
            auto routes = _handlers.find(static_cast<HttpMethod>(httpReq.method));

            if(_quit.load(std::memory_order_acquire) || _asioContextService->fibersShouldStop()) {
                co_return {};
            }

            if (routes != std::end(_handlers)) {
                auto handler = routes->second.find(httpReq.route);

                if (handler != std::end(routes->second)) {

                    // using reference here leads to heap use after free. Not sure why.
                    HttpResponse httpRes = std::move(*co_await handler->second(httpReq).begin());
                    http::response<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>> res{static_cast<http::status>(httpRes.status),
                                                                                                                version};
                    if(_sendServerHeader) {
                        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                    }
                    if(!httpRes.contentType) {
                        res.set(http::field::content_type, "text/plain");
                    } else {
                        res.set(http::field::content_type, *httpRes.contentType);
                    }
                    for (auto const &header: httpRes.headers) {
                        res.set(header.value, header.name);
                    }
                    res.keep_alive(keep_alive);
                    ICHOR_LOG_TRACE_ATOMIC(_logger, "sending http response {} - {}", (int)httpRes.status,
                                    std::string_view(reinterpret_cast<char *>(httpRes.body.data()), httpRes.body.size()));

                    res.body() = std::move(httpRes.body);
                    res.prepare_payload();
                    sendInternal(connection, std::move(res));

                    co_return {};
                }
            }

            http::response<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>> res{http::status::not_found, version};
            if(_sendServerHeader) {
                res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            }
            res.set(http::field::content_type, "text/plain");
            res.keep_alive(keep_alive);
            res.prepare_payload();
            sendInternal(connection, std::move(res));

            co_return {};
        });
    }

    {
        std::lock_guard lg(_streamsMutex);
        _httpStreams.erase(streamId);
    }

    // At this point the connection is closed gracefully
    ICHOR_LOG_WARN_ATOMIC(_logger, "finished read() {} {}", _quit.load(std::memory_order_acquire), _asioContextService->fibersShouldStop());
}

void Ichor::HttpHostService::sendInternal(std::shared_ptr<Detail::Connection> &connection, http::response<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>> &&res) {
    static_assert(std::is_move_assignable_v<Detail::HostOutboxMessage>, "HostOutboxMessage should be move assignable");

    if(_quit.load(std::memory_order_acquire) || _asioContextService->fibersShouldStop()) {
        return;
    }

    net::spawn(*_asioContextService->getContext(), [this, res = std::move(res), connection = std::move(connection)](net::yield_context yield) mutable {
        if(_quit.load(std::memory_order_acquire)) {
            return;
        }

        std::unique_lock lg(connection->mutex);
        if(connection->outbox.full()) {
            connection->outbox.set_capacity(std::max<uint64_t>(connection->outbox.capacity() * 2, 10ul));
        }
        connection->outbox.push_back(std::move(res));
        if(connection->outbox.size() > 1) {
            // handled by existing net::spawn
            return;
        }

        while(!_quit.load(std::memory_order_acquire) && !connection->outbox.empty()) {
            // Move message, should be trivially copyable and prevents iterator invalidation
            auto next = std::move(connection->outbox.front());
            lg.unlock();
            connection->socket.expires_after(30s);
            beast::error_code ec;
            http::async_write(connection->socket, next, yield[ec]);
            if (ec == http::error::end_of_stream) {
                fail(ec, "HttpHostService::sendInternal end of stream", false);
            } else if (ec == net::error::operation_aborted) {
                fail(ec, "HttpHostService::sendInternal operation aborted", false);
            } else if (ec == net::error::bad_descriptor) {
                fail(ec, "HttpHostService::sendInternal bad descriptor", false);
            } else if (ec) {
                fail(ec, "HttpHostService::sendInternal write", false);
            }
            lg.lock();
            connection->outbox.pop_front();
        }
    }ASIO_SPAWN_COMPLETION_TOKEN);
}

#endif
