#include <ichor/DependencyManager.h>
#include <ichor/services/network/boost/HttpHostService.h>
#include <ichor/services/network/http/HttpScopeGuards.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/ServiceExecutionScope.h>

Ichor::Boost::v1::HttpHostService::HttpHostService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
    reg.registerDependency<Ichor::v1::ILogger>(this, DependencyFlags::REQUIRED);
    reg.registerDependency<IBoostAsioQueue>(this, DependencyFlags::REQUIRED);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::Boost::v1::HttpHostService::start() {
    auto addrIt = getProperties().find("Address");
    auto portIt = getProperties().find("Port");

    if(addrIt == getProperties().end()) {
        ICHOR_LOG_ERROR(_logger, "Missing address");
        co_return tl::unexpected(StartError::FAILED);
    }
    if(portIt == getProperties().end()) {
        ICHOR_LOG_ERROR(_logger, "Missing port");
        co_return tl::unexpected(StartError::FAILED);
    }

    if(auto propIt = getProperties().find("Priority"); propIt != getProperties().end()) {
        _priority = Ichor::v1::any_cast<uint64_t>(propIt->second);
    }
    if(auto propIt = getProperties().find("Debug"); propIt != getProperties().end()) {
        _debug = Ichor::v1::any_cast<bool>(propIt->second);
    }
    if(auto propIt = getProperties().find("SendServerHeader"); propIt != getProperties().end()) {
        _sendServerHeader = Ichor::v1::any_cast<bool>(propIt->second);
    }
    if(auto propIt = getProperties().find("NoDelay"); propIt != getProperties().end()) {
        _tcpNoDelay = Ichor::v1::any_cast<bool>(propIt->second);
    }
    if(auto propIt = getProperties().find("SslCert"); propIt != getProperties().end()) {
        _useSsl = true;
    }

    auto sslKeyIt = getProperties().find("SslKey");

    if((_useSsl && sslKeyIt == getProperties().end()) ||
        (!_useSsl && sslKeyIt != getProperties().end())) {
        ICHOR_LOG_ERROR(_logger, "Both SslCert and SslKey properties are required when using ssl");
        co_return tl::unexpected(StartError::FAILED);
    }

    boost::system::error_code ec;
    auto address = net::ip::make_address(Ichor::v1::any_cast<std::string &>(addrIt->second), ec);
    auto port = Ichor::v1::any_cast<uint16_t>(portIt->second);

    if(ec) {
        ICHOR_LOG_ERROR(_logger, "Couldn't parse address \"{}\": {} {}", Ichor::v1::any_cast<std::string &>(addrIt->second), ec.value(), ec.message());
        co_return tl::unexpected(StartError::FAILED);
    }

    net::spawn(_queue->getContext(), [this, address = std::move(address), port](net::yield_context yield) {
        listen(tcp::endpoint{address, port}, std::move(yield));
    }ASIO_SPAWN_COMPLETION_TOKEN);

    co_return {};
}

Ichor::Task<void> Ichor::Boost::v1::HttpHostService::stop() {
    INTERNAL_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! HttpHostService::stop()");
    _quit = true;

    if(!_goingToCleanupStream && _httpAcceptor) {
        _goingToCleanupStream = true;
        if(_httpAcceptor->is_open()) {
            _httpAcceptor->close();
        }

        for (auto &[id, stream]: _httpStreams) {
            stream->socket.cancel();
        }
        for (auto &[id, stream]: _sslStreams) {
            beast::get_lowest_layer(stream->socket).cancel();
        }

        _httpAcceptor = nullptr;

        _startStopEvent.set();
    }

    co_await _startStopEvent;
    INTERNAL_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! post await");

    while(_finishedListenAndRead.load(std::memory_order_acquire) != 0) {
        _startStopEvent.reset();
        _queue->pushEvent<RunFunctionEvent>(getServiceId(), [this]() {
            _startStopEvent.set();
        });
        co_await _startStopEvent;
    }

    _httpStreams.clear();
    INTERNAL_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! done");

    co_return;
}

void Ichor::Boost::v1::HttpHostService::addDependencyInstance(Ichor::ScopedServiceProxy<Ichor::v1::ILogger*> logger, IService &) {
    _logger = std::move(logger);
}

void Ichor::Boost::v1::HttpHostService::removeDependencyInstance(Ichor::ScopedServiceProxy<Ichor::v1::ILogger*>, IService&) {
    _logger = nullptr;
}

void Ichor::Boost::v1::HttpHostService::addDependencyInstance(Ichor::ScopedServiceProxy<IBoostAsioQueue*> q, IService&) {
    _queue = std::move(q);
}

void Ichor::Boost::v1::HttpHostService::removeDependencyInstance(Ichor::ScopedServiceProxy<IBoostAsioQueue*>, IService&) {
    _queue = nullptr;
}

void Ichor::Boost::v1::HttpHostService::setPriority(uint64_t priority) {
    _priority = priority;
}

uint64_t Ichor::Boost::v1::HttpHostService::getPriority() {
    return _priority;
}

Ichor::v1::HttpRouteRegistration Ichor::Boost::v1::HttpHostService::addRoute(Ichor::v1::HttpMethod method, std::string_view route, std::function<Task<Ichor::v1::HttpResponse>(Ichor::v1::HttpRequest&)> handler) {
    return addRoute(method, std::make_unique<Ichor::v1::StringRouteMatcher>(route), std::move(handler));
}

Ichor::v1::HttpRouteRegistration Ichor::Boost::v1::HttpHostService::addRoute(Ichor::v1::HttpMethod method, std::unique_ptr<Ichor::v1::RouteMatcher> newMatcher, std::function<Task<Ichor::v1::HttpResponse>(Ichor::v1::HttpRequest&)> handler) {
    auto routes = _handlers.find(method);

    newMatcher->set_id(_matchersIdCounter);

    if(routes == _handlers.end()) {
        unordered_map<std::unique_ptr<Ichor::v1::RouteMatcher>, std::function<Task<Ichor::v1::HttpResponse>(Ichor::v1::HttpRequest&)>> newSubMap{};
        newSubMap.emplace(std::move(newMatcher), std::move(handler));
        _handlers.emplace(method, std::move(newSubMap));
    } else {
        routes->second.emplace(std::move(newMatcher), std::move(handler));
    }

    return {method, _matchersIdCounter++, this};
}

void Ichor::Boost::v1::HttpHostService::removeRoute(Ichor::v1::HttpMethod method, Ichor::v1::RouteIdType id) {
    auto routes = _handlers.find(method);

    if(routes == std::end(_handlers)) {
        return;
    }

    std::erase_if(routes->second, [id](auto const &item) {
       return item.first->get_id() == id;
    });
}

void Ichor::Boost::v1::HttpHostService::fail(beast::error_code ec, const char *what, bool stopSelf) {
    ICHOR_LOG_ERROR(_logger, "Boost.BEAST fail: {}, {}", what, ec.message());
    if(stopSelf) {
        _queue->pushPrioritisedEvent<StopServiceEvent>(getServiceId(), _priority, getServiceId());
    }
}

void Ichor::Boost::v1::HttpHostService::listen(tcp::endpoint endpoint, net::yield_context yield) {
    Ichor::v1::ScopeGuardAtomicCount guard{_finishedListenAndRead};
    beast::error_code ec;

    if(_useSsl) {
        _sslContext = std::make_unique<net::ssl::context>(net::ssl::context::tlsv12);

        if(auto propIt = getProperties().find("SslPassword"); propIt != getProperties().end()) {
            _sslContext->set_password_callback(
                    [password = Ichor::v1::any_cast<std::string>(propIt->second)](std::size_t, boost::asio::ssl::context_base::password_purpose) {
                        return password;
                    });
        }

        _sslContext->set_options(boost::asio::ssl::context::default_workarounds |
                                 boost::asio::ssl::context::no_tlsv1 |
                                 boost::asio::ssl::context::no_tlsv1_1 |
                                 boost::asio::ssl::context::no_sslv2 |
                                 boost::asio::ssl::context::no_sslv3);

        //pretty sure that Boost.Beast uses references to the data/size, so we should make sure to keep this in memory until we're done.
        auto& cert = Ichor::v1::any_cast<std::string&>(getProperties()["SslCert"]);
        auto& key = Ichor::v1::any_cast<std::string&>(getProperties()["SslKey"]);
        _sslContext->use_certificate_chain(boost::asio::buffer(cert.data(), cert.size()));
        _sslContext->use_private_key(boost::asio::buffer(key.data(), key.size()), boost::asio::ssl::context::file_format::pem);
    }

    _httpAcceptor = std::make_unique<tcp::acceptor>(_queue->getContext());
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

    while(!_quit && !_queue->fibersShouldStop())
    {
        auto socket = tcp::socket(_queue->getContext());

        // tcp accept new connections
        _httpAcceptor->async_accept(socket, yield[ec]);
        if(ec)
        {
            fail(ec, "HttpHostService::listen accept", false);
            continue;
        }

        socket.set_option(tcp::no_delay(_tcpNoDelay));

        net::spawn(_queue->getContext(), [this, socket = std::move(socket)](net::yield_context _yield) mutable {
            if(_quit) {
                return;
            }

            if(_useSsl) {
                read<beast::ssl_stream<beast::tcp_stream>>(std::move(socket), std::move(_yield));
            } else {
                read<beast::tcp_stream>(std::move(socket), std::move(_yield));
            }
        }ASIO_SPAWN_COMPLETION_TOKEN);
    }

    ICHOR_LOG_WARN(_logger, "finished listen() {} {}", _quit, _queue->fibersShouldStop());
    if(!_goingToCleanupStream && _httpAcceptor) {
        _goingToCleanupStream = true;
        _queue->pushPrioritisedEvent<StopServiceEvent>(getServiceId(), getPriority(), getServiceId());
    }
}

template <typename SocketT>
void Ichor::Boost::v1::HttpHostService::read(tcp::socket socket, net::yield_context yield) {
    Ichor::v1::ScopeGuardAtomicCount const guard{ _finishedListenAndRead };
    beast::error_code ec;
    auto addr = socket.remote_endpoint().address().to_string();
    std::shared_ptr<Detail::Connection<SocketT>> connection;
    uint64_t streamId = _streamIdCounter++;
    if constexpr (std::is_same_v<SocketT, beast::tcp_stream>) {
        connection = _httpStreams.emplace(streamId, std::make_shared<Detail::Connection<SocketT>>(std::move(socket))).first->second;
    } else {
        connection = _sslStreams.emplace(streamId, std::make_shared<Detail::Connection<SocketT>>(std::move(socket), *_sslContext)).first->second;
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

    while (!_quit && !_queue->fibersShouldStop())
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

        auto target = std::string{req.target()};
        ICHOR_LOG_TRACE(_logger, "New request for {} {}", (int)req.method(), target);

        unordered_map<std::string, std::string> headers{};
        headers.reserve(static_cast<unsigned long>(std::distance(std::begin(req), std::end(req))));
        for (auto const& field : req) {
            headers.emplace(field.name_string(), field.value());
        }
        // rapidjson f.e. expects a null terminator
        if (!req.body().empty() && *req.body().rbegin() != 0) {
            req.body().push_back(0);
        }
        Ichor::v1::HttpRequest httpReq{ std::move(req.body()), static_cast<Ichor::v1::HttpMethod>(req.method()), target, {}, addr, std::move(headers) };
        // Compiler bug prevents using named captures for now: https://www.reddit.com/r/cpp_questions/comments/17lc55f/coroutine_msvc_compiler_bug/
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
        auto version = req.version();
        auto keep_alive = req.keep_alive();
        _queue->pushEvent<RunFunctionEventAsync>(getServiceId(), [this, connection, httpReq, version, keep_alive]() mutable -> AsyncGenerator<IchorBehaviour> {
#else
        _queue->pushEvent<RunFunctionEventAsync>(getServiceId(), [this, connection, target = std::move(target), httpReq = std::move(httpReq), version = req.version(), keep_alive = req.keep_alive()]() mutable -> AsyncGenerator<IchorBehaviour> {
#endif
            auto routes = _handlers.find(httpReq.method);

            if (_quit || _queue->fibersShouldStop()) {
                co_return{};
            }
            httpReq.route = target;

            if (routes != std::end(_handlers)) {
                std::function<Task<Ichor::v1::HttpResponse>(Ichor::v1::HttpRequest&)> const *f{};
                for(auto const &[matcher, handler] : routes->second) {
                    if(matcher->matches(httpReq.route)) {
                        httpReq.regex_params = matcher->route_params();
                        f = &handler;
                        break;
                    }
                }

                if (f != nullptr) {
                    auto httpRes = co_await (*f)(httpReq);
                    http::response<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>> res{ static_cast<http::status>(httpRes.status), version };
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
                    ICHOR_LOG_TRACE(_logger, "sending http response {} - {}", (int)httpRes.status,
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

    if constexpr (std::is_same_v<SocketT, beast::tcp_stream>) {
        _httpStreams.erase(streamId);
    } else {
        _sslStreams.erase(streamId);
    }

    // At this point the connection is closed gracefully
    ICHOR_LOG_WARN(_logger, "finished read() {} {}", _quit, _queue->fibersShouldStop());
}

template <typename SocketT>
void Ichor::Boost::v1::HttpHostService::sendInternal(std::shared_ptr<Detail::Connection<SocketT>> &connection, http::response<http::vector_body<uint8_t>, http::basic_fields<std::allocator<uint8_t>>> &&res) {
    static_assert(std::is_move_assignable_v<Detail::HostOutboxMessage>, "HostOutboxMessage should be move assignable");

    if(_quit || _queue->fibersShouldStop()) {
        return;
    }

    net::spawn(_queue->getContext(), [this, res = std::move(res), connection = std::move(connection)](net::yield_context yield) mutable {
        if(_quit) {
            return;
        }

        if(connection->outbox.full()) {
            connection->outbox.set_capacity(std::max<uint64_t>(connection->outbox.capacity() * 2, 10ul));
        }
        connection->outbox.push_back(std::move(res));
        if(connection->outbox.size() > 1) {
            // handled by existing net::spawn
            return;
        }

        while(!_quit && !connection->outbox.empty()) {
            // Move message, should be trivially copyable and prevents iterator invalidation
            auto next = std::move(connection->outbox.front());
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
            connection->outbox.pop_front();
        }
    }ASIO_SPAWN_COMPLETION_TOKEN);
}
