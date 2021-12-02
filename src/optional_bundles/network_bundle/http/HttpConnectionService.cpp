#ifdef USE_BOOST_BEAST

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/network_bundle/http/HttpConnectionService.h>


Ichor::HttpConnectionService::HttpConnectionService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
    reg.registerDependency<ILogger>(this, true);
}

bool Ichor::HttpConnectionService::start() {
    if(!_quit && !_connecting && !_connected) {
        ICHOR_LOG_WARN(_logger, "starting svc {}", getServiceId());
        if (getProperties()->contains("Priority")) {
            _priority = Ichor::any_cast<uint64_t>(getProperties()->operator[]("Priority"));
        }

        if (!getProperties()->contains("Port") || !getProperties()->contains("Address")) {
            getManager()->pushPrioritisedEvent<UnrecoverableErrorEvent>(getServiceId(), _priority, 0,
                                                                        "Missing port or address when starting HttpConnectionService");
            return false;
        }

        _httpContext = Ichor::make_unique<net::io_context>(getMemoryResource(), BOOST_ASIO_CONCURRENCY_HINT_UNSAFE);
        auto address = net::ip::make_address(Ichor::any_cast<std::string &>(getProperties()->operator[]("Address")));
        auto port = Ichor::any_cast<uint16_t>(getProperties()->operator[]("Port"));

        _connecting = true;
        net::spawn(*_httpContext, [this, address = std::move(address), port](net::yield_context yield) {
            connect(tcp::endpoint{address, port}, std::move(yield));
        });
    }

    if(_quit) {
        return false;
    }

    _httpContext->poll();

    if(!_connected) {
        getManager()->pushEvent<StartServiceEvent>(getServiceId(), getServiceId());
        return false;
    }

    _timerManager = getManager()->createServiceManager<Timer, ITimer>();
    _timerManager->setChronoInterval(std::chrono::milliseconds(20));
    _timerManager->setCallback([this](TimerEvent const * const evt) -> Generator<bool> {
        _httpContext->poll();
        co_return (bool)PreventOthersHandling;
    });
    _timerManager->startTimer();

    return true;
}

bool Ichor::HttpConnectionService::stop() {
    _quit = true;
    _timerManager = nullptr;

    close();

    while(_httpContext->poll() > 0);

    _httpContext->stop();
    _httpStream = nullptr;
    _httpContext = nullptr;

    return true;
}

void Ichor::HttpConnectionService::addDependencyInstance(ILogger *logger, IService *) {
    _logger = logger;
    ICHOR_LOG_TRACE(_logger, "Inserted logger");
}

void Ichor::HttpConnectionService::removeDependencyInstance(ILogger *logger, IService *) {
    _logger = nullptr;
}

void Ichor::HttpConnectionService::setPriority(uint64_t priority) {
    _priority = priority;
}

uint64_t Ichor::HttpConnectionService::getPriority() {
    return _priority;
}

uint64_t Ichor::HttpConnectionService::sendAsync(Ichor::HttpMethod method, std::string_view route, std::vector<HttpHeader, Ichor::PolymorphicAllocator<HttpHeader>> &&headers, std::vector<uint8_t, Ichor::PolymorphicAllocator<uint8_t>> &&msg) {

    if(method == HttpMethod::get && !msg.empty()) {
        throw std::runtime_error("GET requests cannot have a body.");
    }

    ICHOR_LOG_DEBUG(_logger, "sending to {}", route);

    if(_quit) {
        return 0;
    }

    auto msgId = _msgId++;

    net::spawn(*_httpContext, [this, msgId, method, route, headers = std::forward<decltype(headers)>(headers), msg = std::forward<decltype(msg)>(msg)](net::yield_context yield) mutable {
        beast::error_code ec;
        http::request<http::vector_body<uint8_t, Ichor::PolymorphicAllocator<uint8_t>>, http::basic_fields<Ichor::PolymorphicAllocator<uint8_t>>> req{static_cast<http::verb>(method), route, 11, std::move(msg)};

        for(auto const &header : headers) {
            req.set(header.name, header.value);
        }
        req.set(http::field::host, Ichor::any_cast<std::string &>(getProperties()->operator[]("Address")));
        req.prepare_payload();

        http::write(*_httpStream, req, ec);
        if (ec) {
            fail(ec, "HttpConnectionService::sendAsync write");
        }

        // This buffer is used for reading and must be persisted
        beast::flat_buffer b;

        // Declare a container to hold the response
        http::response<http::vector_body<uint8_t, Ichor::PolymorphicAllocator<uint8_t>>, http::basic_fields<Ichor::PolymorphicAllocator<uint8_t>>> res;

        // Receive the HTTP response
        http::async_read(*_httpStream, b, res, yield[ec]);
        if(ec) {
            return fail(ec, "HttpConnectionService::sendAsync read");
        }

        ICHOR_LOG_WARN(_logger, "received HTTP response {}", std::string_view(reinterpret_cast<char*>(res.body().data()), res.body().size()));

        std::vector<HttpHeader, Ichor::PolymorphicAllocator<HttpHeader>> resHeaders;
        resHeaders.reserve(std::distance(std::begin(res), std::end(res)));
        for(auto const &header : res) {
            resHeaders.emplace_back(header.name_string(), header.value());
        }

        // need to use move iterator instead of std::move directly, to prevent leaks.
        std::vector<uint8_t, Ichor::PolymorphicAllocator<uint8_t>> body;
        body.insert(body.end(), std::make_move_iterator(res.body().begin()), std::make_move_iterator(res.body().end()));

        getManager()->pushPrioritisedEvent<HttpResponseEvent>(getServiceId(), getPriority(), msgId, HttpResponse{static_cast<HttpStatus>(res.result()), std::move(body), std::move(resHeaders)});
    });

    return msgId;
}

bool Ichor::HttpConnectionService::close() {
    if(_quit) {
        net::spawn(*_httpContext, [this](net::yield_context yield) {
            beast::error_code ec;
            _httpStream->socket().shutdown(tcp::socket::shutdown_both, ec);

            if (ec && ec != beast::errc::not_connected) {
                fail(ec, "HttpConnectionService::close shutdown");
            }
        });

        return true;
    }

    return false;
}

void Ichor::HttpConnectionService::fail(beast::error_code ec, const char *what) {
    ICHOR_LOG_ERROR(_logger, "Boost.BEAST fail: {}, {}", what, ec.message());
    getManager()->pushPrioritisedEvent<StopServiceEvent>(getServiceId(), _priority, getServiceId());
}

void Ichor::HttpConnectionService::connect(tcp::endpoint endpoint, net::yield_context yield) {
    beast::error_code ec;

    tcp::resolver resolver(*_httpContext);
    _httpStream = Ichor::make_unique<beast::tcp_stream>(getMemoryResource(), *_httpContext);

    auto const results = resolver.resolve(endpoint, ec);
    if(ec) {
        return fail(ec, "HttpConnectionService::connect resolve");
    }

    // Set the timeout.
    _httpStream->expires_after(std::chrono::seconds(30));

    // Make the connection on the IP address we get from a lookup
    while(!_quit && _attempts < 5) {
//        ICHOR_LOG_WARN(_logger, "async_connect {}", _attempts);
        _httpStream->async_connect(results, yield[ec]);
//        ICHOR_LOG_WARN(_logger, "async_connect after {}", _attempts);
        if (ec) {
            _attempts++;
            net::steady_timer t{*_httpContext};
            t.expires_after(std::chrono::milliseconds(250));
            t.async_wait(yield);
        } else {
            break;
        }
    }

    if(ec) {
        _quit = true;
        _connecting = false;
        return fail(ec, "HttpConnectionService::connect connect");
    }

    _connecting = false;
    _connected = true;
    while(!_quit) {
        net::steady_timer t{*_httpContext};
        t.expires_after(std::chrono::milliseconds(50));
        t.async_wait(yield);
    }
}

#endif