#ifdef USE_BOOST_BEAST

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/network_bundle/http/HttpHostService.h>


Ichor::HttpHostService::HttpHostService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
    reg.registerDependency<ILogger>(this, true);
    reg.registerDependency<IHttpContextService>(this, true);
}

Ichor::StartBehaviour Ichor::HttpHostService::start() {
    if(getProperties()->contains("Priority")) {
        _priority = Ichor::any_cast<uint64_t>(getProperties()->operator[]("Priority"));
    }

    if(!getProperties()->contains("Port") || !getProperties()->contains("Address")) {
        getManager()->pushPrioritisedEvent<UnrecoverableErrorEvent>(getServiceId(), _priority, 0, "Missing port or address when starting HttpHostService");
        return Ichor::StartBehaviour::FAILED_DO_NOT_RETRY;
    }

    if(getProperties()->contains("NoDelay")) {
        _tcpNoDelay = Ichor::any_cast<bool>(getProperties()->operator[]("NoDelay"));
    }

    auto address = net::ip::make_address(Ichor::any_cast<std::string&>(getProperties()->operator[]("Address")));
    auto port = Ichor::any_cast<uint16_t>(getProperties()->operator[]("Port"));

    net::spawn(*_httpContextService->getContext(), [this, address = std::move(address), port](net::yield_context yield){
        listen(tcp::endpoint{address, port}, std::move(yield));
    });

    return Ichor::StartBehaviour::SUCCEEDED;
}

Ichor::StartBehaviour Ichor::HttpHostService::stop() {
    _quit = true;

    _httpAcceptor->close();
    _httpStream->cancel();

    _httpAcceptor = nullptr;
    _httpStream = nullptr;

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
    _httpContextService = nullptr;
    _quit = true;
}

void Ichor::HttpHostService::setPriority(uint64_t priority) {
    _priority.store(priority, std::memory_order_release);
}

uint64_t Ichor::HttpHostService::getPriority() {
    return _priority.load(std::memory_order_acquire);
}

void Ichor::HttpHostService::fail(beast::error_code ec, const char *what) {
    ICHOR_LOG_ERROR(_logger, "Boost.BEAST fail: {}, {}", what, ec.message());
    getManager()->pushPrioritisedEvent<StopServiceEvent>(getServiceId(), _priority.load(std::memory_order_acquire), getServiceId());
}

void Ichor::HttpHostService::listen(tcp::endpoint endpoint, net::yield_context yield)
{
    beast::error_code ec;

    _httpAcceptor = Ichor::make_unique<tcp::acceptor>(getMemoryResource(), *_httpContextService->getContext());
    _httpAcceptor->open(endpoint.protocol(), ec);
    if(ec) {
        return fail(ec, "HttpHostService::listen open");
    }

    _httpAcceptor->set_option(net::socket_base::reuse_address(true), ec);
    if(ec) {
        return fail(ec, "HttpHostService::listen set_option");
    }

    _httpAcceptor->bind(endpoint, ec);
    if(ec) {
        return fail(ec, "HttpHostService::listen bind");
    }

    _httpAcceptor->listen(net::socket_base::max_listen_connections, ec);
    if(ec) {
        return fail(ec, "HttpHostService::listen listen");
    }

    while(!_quit && !_httpContextService->fibersShouldStop())
    {
        auto socket = tcp::socket(*_httpContextService->getContext());

        // tcp accept new connections
        _httpAcceptor->async_accept(socket, yield[ec]);
        if(ec)
        {
            fail(ec, "HttpHostService::listen accept");
            continue;
        }

        socket.set_option(tcp::no_delay(_tcpNoDelay));

        net::spawn(*_httpContextService->getContext(), [this, socket = std::move(socket)](net::yield_context _yield) mutable {
            read(std::forward<decltype(socket)>(socket), std::move(_yield));
        });
    }
    ICHOR_LOG_WARN(_logger, "finished listen()");
}

Ichor::unique_ptr<Ichor::HttpRouteRegistration> Ichor::HttpHostService::addRoute(HttpMethod method, std::string_view route, std::function<HttpResponse(HttpRequest&)> handler) {
    auto &routes = _handlers[method];

    auto existingHandler = routes.find(route);

    if(existingHandler != std::end(routes)) {
        throw std::runtime_error("Route already present in handlers");
    }

    auto insertedIt = routes.emplace(route, handler);

    // convoluted way to pass a string_view that doesn't go out of scope after this function
    return Ichor::make_unique<HttpRouteRegistration>(getMemoryResource(), method, insertedIt.first->first, this);
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

void Ichor::HttpHostService::read(tcp::socket socket, net::yield_context yield) {
    beast::error_code ec;
    _httpStream = Ichor::make_unique<beast::tcp_stream>(getMemoryResource(), std::move(socket));

    // This buffer is required to persist across reads
    beast::basic_flat_buffer buffer{Ichor::PolymorphicAllocator<uint8_t>{getMemoryResource()}};

    while(!_quit && !_httpContextService->fibersShouldStop())
    {
        // Set the timeout.
        _httpStream->expires_after(std::chrono::seconds(30));

        // Read a request
        http::request<http::vector_body<uint8_t, Ichor::PolymorphicAllocator<uint8_t>>, http::basic_fields<Ichor::PolymorphicAllocator<uint8_t>>> req;
        http::async_read(*_httpStream, buffer, req, yield[ec]);
        if(ec == http::error::end_of_stream || ec == net::error::operation_aborted) {
            break;
        }
        if(ec) {
            return fail(ec, "HttpHostService::read read");
        }

        ICHOR_LOG_TRACE(_logger, "New request for {} {}", req.method(), req.target());

        auto routes = _handlers.find(static_cast<HttpMethod>(req.method()));

        if(routes != std::end(_handlers)) {
            auto handler = routes->second.find(req.target());

            if(handler != std::end(routes->second)) {
                std::vector<HttpHeader, Ichor::PolymorphicAllocator<HttpHeader>> headers{getMemoryResource()};
                headers.reserve(std::distance(std::begin(req), std::end(req)));
                for(auto const &field : req) {
                    headers.emplace_back(field.name_string(), field.value());
                }
                HttpRequest httpReq{std::move(req.body()), static_cast<HttpMethod>(req.method()), req.target(), std::move(headers)};
                auto httpRes = handler->second(httpReq);
                http::response<http::vector_body<uint8_t, Ichor::PolymorphicAllocator<uint8_t>>, http::basic_fields<Ichor::PolymorphicAllocator<uint8_t>>> res{static_cast<http::status>(httpRes.status), req.version()};
                res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                res.set(http::field::content_type, "text/html");
                for(auto const& header : httpRes.headers) {
                    res.set(header.value, header.name);
                }
                res.keep_alive(req.keep_alive());
                ICHOR_LOG_WARN(_logger, "sending http response {} - {}", httpRes.status, std::string_view(reinterpret_cast<char*>(httpRes.body.data()), httpRes.body.size()));

                res.body() = std::move(httpRes.body);
                res.prepare_payload();
                http::async_write(*_httpStream, res, yield[ec]);
                if(ec == http::error::end_of_stream || ec == net::error::operation_aborted) {
                    break;
                }
                if(ec) {
                    return fail(ec, "HttpHostService::read write");
                }

                continue;
            }
        }

        http::response<http::basic_string_body<char, std::char_traits<char>, Ichor::PolymorphicAllocator<char>>> res{http::status::not_found, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
//        res.body() = std::pmr::string("");
        res.prepare_payload();
        http::async_write(*_httpStream, res, yield[ec]);
        if(ec == http::error::end_of_stream || ec == net::error::operation_aborted) {
            break;
        }
        if(ec) {
            return fail(ec, "HttpHostService::read write2");
        }
    }

    // Send a TCP shutdown
    _httpStream->socket().shutdown(tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
    ICHOR_LOG_WARN(_logger, "finished read()");
}

#endif