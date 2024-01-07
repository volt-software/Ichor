#include <ichor/DependencyManager.h>
#include <ichor/services/network/boost/HttpHostService.h>
#include <ichor/services/network/http/HttpScopeGuards.h>
#include <ichor/events/RunFunctionEvent.h>

Ichor::HttpHostService::HttpHostService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
    reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
    reg.registerDependency<IAsioContextService>(this, DependencyFlags::REQUIRED);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::HttpHostService::start() {
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

    if(getProperties().contains("SslCert")) {
        _useSsl.store(true, std::memory_order_release);
    }

    if((_useSsl.load(std::memory_order_acquire) && !getProperties().contains("SslKey")) ||
        (!_useSsl.load(std::memory_order_acquire) && getProperties().contains("SslKey"))) {
        ICHOR_LOG_ERROR_ATOMIC(_logger, "Both SslCert and SslKey properties are required when using ssl");
        co_return tl::unexpected(StartError::FAILED);
    }

    _queue = &GetThreadLocalEventQueue();

    boost::system::error_code ec;
    auto address = net::ip::make_address(Ichor::any_cast<std::string &>(getProperties()["Address"]), ec);
    auto port = Ichor::any_cast<uint16_t>(getProperties()["Port"]);

    if(ec) {
        ICHOR_LOG_ERROR_ATOMIC(_logger, "Couldn't parse address \"{}\": {} {}", Ichor::any_cast<std::string &>(getProperties()["Address"]), ec.value(), ec.message());
        co_return tl::unexpected(StartError::FAILED);
    }

    net::spawn(*_asioContextService->getContext(), [this, address = std::move(address), port](net::yield_context yield) {
        listen(tcp::endpoint{address, port}, std::move(yield));
    }ASIO_SPAWN_COMPLETION_TOKEN);

    co_return {};
}

Ichor::Task<void> Ichor::HttpHostService::stop() {
    INTERNAL_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! HttpHostService::stop()");
    _quit.store(true, std::memory_order_release);

    if(!_goingToCleanupStream.exchange(true, std::memory_order_acq_rel) && _httpAcceptor) {
        if(_httpAcceptor->is_open()) {
            _httpAcceptor->close();
        }
        net::spawn(*_asioContextService->getContext(), [this](net::yield_context _yield) {
            ScopeGuardAtomicCount guard{_finishedListenAndRead};
            {
                std::unique_lock lg{_streamsMutex};
                for (auto &[id, stream]: _httpStreams) {
                    stream->socket.cancel();
                }
                for (auto &[id, stream]: _sslStreams) {
                    beast::get_lowest_layer(stream->socket).cancel();
                }
            }

            _httpAcceptor = nullptr;

            _queue->pushEvent<RunFunctionEvent>(getServiceId(), [this]() {
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

void Ichor::HttpHostService::addDependencyInstance(ILogger &logger, IService &) {
    _logger.store(&logger, std::memory_order_release);
}

void Ichor::HttpHostService::removeDependencyInstance(ILogger &logger, IService&) {
    _logger.store(nullptr, std::memory_order_release);
}

void Ichor::HttpHostService::addDependencyInstance(IAsioContextService &AsioContextService, IService&) {
    _asioContextService = &AsioContextService;
    ICHOR_LOG_TRACE_ATOMIC(_logger, "Inserted AsioContextService");
}

void Ichor::HttpHostService::removeDependencyInstance(IAsioContextService&, IService&) {
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
        _queue->pushPrioritisedEvent<StopServiceEvent>(getServiceId(), _priority.load(std::memory_order_acquire), getServiceId());
    }
}

void Ichor::HttpHostService::listen(tcp::endpoint endpoint, net::yield_context yield) {
    ScopeGuardAtomicCount guard{_finishedListenAndRead};
    beast::error_code ec;

    if(_useSsl.load(std::memory_order_acquire)) {
        _sslContext = std::make_unique<net::ssl::context>(net::ssl::context::tlsv12);

        if(getProperties().contains("SslPassword")) {
            _sslContext->set_password_callback(
                    [password = Ichor::any_cast<std::string>(getProperties()["SslPassword"])](std::size_t, boost::asio::ssl::context_base::password_purpose) {
                        return password;
                    });
        }

        _sslContext->set_options(boost::asio::ssl::context::default_workarounds |
                                 boost::asio::ssl::context::no_tlsv1 |
                                 boost::asio::ssl::context::no_tlsv1_1 |
                                 boost::asio::ssl::context::no_sslv2 |
                                 boost::asio::ssl::context::no_sslv3);

        //pretty sure that Boost.Beast uses references to the data/size, so we should make sure to keep this in memory until we're done.
        auto& cert = Ichor::any_cast<std::string&>(getProperties()["SslCert"]);
        auto& key = Ichor::any_cast<std::string&>(getProperties()["SslKey"]);
        _sslContext->use_certificate_chain(boost::asio::buffer(cert.data(), cert.size()));
        _sslContext->use_private_key(boost::asio::buffer(key.data(), key.size()), boost::asio::ssl::context::file_format::pem);
    }

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

            if(_useSsl) {
                read<beast::ssl_stream<beast::tcp_stream>>(std::move(socket), std::move(_yield));
            } else {
                read<beast::tcp_stream>(std::move(socket), std::move(_yield));
            }
        }ASIO_SPAWN_COMPLETION_TOKEN);
    }

    ICHOR_LOG_WARN_ATOMIC(_logger, "finished listen() {} {}", _quit.load(std::memory_order_acquire), _asioContextService->fibersShouldStop());
    if(!_goingToCleanupStream.exchange(true, std::memory_order_acq_rel) && _httpAcceptor) {
        _queue->pushPrioritisedEvent<StopServiceEvent>(getServiceId(), getPriority(), getServiceId());
    }
}

template <typename SocketT>
void Ichor::HttpHostService::read(tcp::socket socket, net::yield_context yield) {
    ScopeGuardAtomicCount guard{ _finishedListenAndRead };
    beast::error_code ec;
    auto addr = socket.remote_endpoint().address().to_string();
    std::shared_ptr<Detail::Connection<SocketT>> connection;
    uint64_t streamId;
    {
        std::lock_guard lg(_streamsMutex);
        streamId = _streamIdCounter++;
        if constexpr (std::is_same_v<SocketT, beast::tcp_stream>) {
            connection = _httpStreams.emplace(streamId, std::make_shared<Detail::Connection<SocketT>>(std::move(socket))).first->second;
        } else {
            connection = _sslStreams.emplace(streamId, std::make_shared<Detail::Connection<SocketT>>(std::move(socket), *_sslContext)).first->second;
        }
    }

    if constexpr (!std::is_same_v<SocketT, beast::tcp_stream>) {
        connection->socket.async_handshake(ssl::stream_base::server, yield[ec]);
    }

    if (ec) {
        fail(ec, "HttpHostService::read ssl handshake", false);
        return;
    }

    // This buffer is required to persist across reads
    beast::basic_flat_buffer buffer{ std::allocator<uint8_t>{} };

    while (!_quit.load(std::memory_order_acquire) && !_asioContextService->fibersShouldStop())
    {
        // Set the timeout.
        if constexpr (std::is_same_v<SocketT, beast::tcp_stream>) {
            connection->socket.expires_after(30s);
        } else {
            beast::get_lowest_layer(connection->socket).expires_after(30s);
        }

        // Read a request
        http::request<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>> req;
        http::async_read(connection->socket, buffer, req, yield[ec]);
        if (ec == http::error::end_of_stream) {
            fail(ec, "HttpHostService::read end of stream", false);
            break;
        }
        if (ec == net::error::operation_aborted) {
            fail(ec, "HttpHostService::read operation aborted", false);
            break;
        }
        if (ec == net::error::timed_out) {
            fail(ec, "HttpHostService::read operation timed out", false);
            break;
        }
        if (ec == net::error::bad_descriptor) {
            fail(ec, "HttpHostService::read bad descriptor", false);
            break;
        }
        if (ec) {
            fail(ec, "HttpHostService::read read", false);
            continue;
        }

        ICHOR_LOG_TRACE_ATOMIC(_logger, "New request for {} {}", (int)req.method(), req.target());

        unordered_map<std::string, std::string> headers{};
        headers.reserve(static_cast<unsigned long>(std::distance(std::begin(req), std::end(req))));
        for (auto const& field : req) {
            headers.emplace(field.name_string(), field.value());
        }
        // rapidjson f.e. expects a null terminator
        if (!req.body().empty() && *req.body().rbegin() != 0) {
            req.body().push_back(0);
        }
        HttpRequest httpReq{ std::move(req.body()), static_cast<HttpMethod>(req.method()), std::string{req.target()}, addr, std::move(headers) };
        // Compiler bug prevents using named captures for now: https://www.reddit.com/r/cpp_questions/comments/17lc55f/coroutine_msvc_compiler_bug/
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
        auto version = req.version();
        auto keep_alive = req.keep_alive();
        _queue->pushEvent<RunFunctionEventAsync>(getServiceId(), [this, connection, httpReq, version, keep_alive]() mutable -> AsyncGenerator<IchorBehaviour> {
#else
        _queue->pushEvent<RunFunctionEventAsync>(getServiceId(), [this, connection, httpReq = std::move(httpReq), version = req.version(), keep_alive = req.keep_alive()]() mutable -> AsyncGenerator<IchorBehaviour> {
#endif
            auto routes = _handlers.find(static_cast<HttpMethod>(httpReq.method));

            if (_quit.load(std::memory_order_acquire) || _asioContextService->fibersShouldStop()) {
                co_return{};
            }

            if (routes != std::end(_handlers)) {
                auto handler = routes->second.find(httpReq.route);

                if (handler != std::end(routes->second)) {

                    // using reference here leads to heap use after free. Not sure why.
                    auto gen = handler->second(httpReq);
                    auto it = co_await gen.begin();
                    HttpResponse httpRes = std::move(*it);
                    http::response<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>> res{ static_cast<http::status>(httpRes.status),
                                                                                                                version };
                    if (_sendServerHeader) {
                        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                    }
                    if (!httpRes.contentType) {
                        res.set(http::field::content_type, "text/plain");
                    } else {
                        res.set(http::field::content_type, *httpRes.contentType);
                    }
                    for (auto const& header : httpRes.headers) {
                        res.set(header.first, header.second);
                    }
                    res.keep_alive(keep_alive);
                    ICHOR_LOG_TRACE_ATOMIC(_logger, "sending http response {} - {}", (int)httpRes.status,
                        std::string_view(reinterpret_cast<char*>(httpRes.body.data()), httpRes.body.size()));

                    res.body() = std::move(httpRes.body);
                    res.prepare_payload();
                    sendInternal(connection, std::move(res));

                    co_return{};
                }
            }

            http::response<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>> res{ http::status::not_found, version };
            if (_sendServerHeader) {
                res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            }
            res.set(http::field::content_type, "text/plain");
            res.keep_alive(keep_alive);
            res.prepare_payload();
            sendInternal(connection, std::move(res));

            co_return{};
        });
    }

    {
        std::lock_guard lg(_streamsMutex);
        if constexpr (std::is_same_v<SocketT, beast::tcp_stream>) {
            _httpStreams.erase(streamId);
        } else {
            _sslStreams.erase(streamId);
        }
    }

    // At this point the connection is closed gracefully
    ICHOR_LOG_WARN_ATOMIC(_logger, "finished read() {} {}", _quit.load(std::memory_order_acquire), _asioContextService->fibersShouldStop());
}

template <typename SocketT>
void Ichor::HttpHostService::sendInternal(std::shared_ptr<Detail::Connection<SocketT>> &connection, http::response<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>> &&res) {
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
            if constexpr (std::is_same_v<SocketT, beast::tcp_stream>) {
                connection->socket.expires_after(30s);
            } else {
                beast::get_lowest_layer(connection->socket).expires_after(30s);
            }
            beast::error_code ec;
            http::async_write(connection->socket, next, yield[ec]);
            if(ec == http::error::end_of_stream) {
                fail(ec, "HttpHostService::sendInternal end of stream", false);
            } else if(ec == net::error::operation_aborted) {
                fail(ec, "HttpHostService::sendInternal operation aborted", false);
            } else if(ec == net::error::bad_descriptor) {
                fail(ec, "HttpHostService::sendInternal bad descriptor", false);
            } else if(ec) {
                fail(ec, "HttpHostService::sendInternal write", false);
            }
            lg.lock();
            connection->outbox.pop_front();
        }
    }ASIO_SPAWN_COMPLETION_TOKEN);
}
