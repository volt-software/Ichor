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

        _httpContext = std::make_unique<net::io_context>(1);
        auto address = net::ip::make_address(Ichor::any_cast<std::string &>(getProperties()->operator[]("Address")));
        auto port = Ichor::any_cast<uint16_t>(getProperties()->operator[]("Port"));

        _connecting = true;
        net::spawn(*_httpContext, [this, address = std::move(address), port](net::yield_context yield) {
            connect(tcp::endpoint{address, port}, std::move(yield));
        });

        _listenThread = std::thread([this] {
            _httpContext->run();
        });

#ifdef __linux__
        pthread_setname_np(_listenThread.native_handle(), fmt::format("HtConn #{}", getServiceId()).c_str());
#endif
    }

    if(_quit) {
        return false;
    }

    if(!_connected) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        getManager()->pushEvent<StartServiceEvent>(getServiceId(), getServiceId());
        return false;
    }

    return true;
}

bool Ichor::HttpConnectionService::stop() {
    close();

    _listenThread.join();

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
    _priority.store(priority, std::memory_order_release);
}

uint64_t Ichor::HttpConnectionService::getPriority() {
    return _priority.load(std::memory_order_acquire);
}

uint64_t Ichor::HttpConnectionService::sendAsync(Ichor::HttpMethod method, std::string_view route, std::vector<HttpHeader> &&headers, std::pmr::vector<uint8_t> &&msg) {

    if(method == HttpMethod::get && !msg.empty()) {
        throw std::runtime_error("GET requests cannot have a body.");
    }

    auto msgId = _msgId.fetch_add(1, std::memory_order_acq_rel);
    if(!_quit) {
        net::spawn(*_httpContext, [this, msgId, method, route, headers = std::forward<decltype(headers)>(headers), msg = std::forward<decltype(msg)>(msg)](net::yield_context yield) mutable {
            beast::error_code ec;
            http::request<http::vector_body<uint8_t, std::pmr::polymorphic_allocator<uint8_t>>> req{static_cast<http::verb>(method), route, 11, std::move(msg)};

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
            http::response<http::vector_body<uint8_t, std::pmr::polymorphic_allocator<uint8_t>>> res;

            // Receive the HTTP response
            http::read(*_httpStream, b, res, ec);
            if(ec) {
                return fail(ec, "HttpConnectionService::sendAsync read");
            }

//            ICHOR_LOG_WARN(_logger, "received HTTP response {}", res.body().data());

            std::pmr::vector<HttpHeader> resHeaders;
            resHeaders.reserve(10);
            for(auto const &header : res) {
                resHeaders.emplace_back(std::string{header.name_string()}, std::string{header.value()});
            }

            getManager()->pushPrioritisedEvent<HttpResponseEvent>(getServiceId(), getPriority(), msgId, HttpResponse{static_cast<HttpStatus>(res.result()), std::move(res.body()), std::move(resHeaders)});
        });
    }
    return msgId;
}

bool Ichor::HttpConnectionService::close() {
    bool expected = false;
    if(_quit.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        net::spawn(*_httpContext, [this](net::yield_context yield) {
            beast::error_code ec;
            _httpStream->socket().shutdown(tcp::socket::shutdown_both, ec);

            if (ec && ec != beast::errc::not_connected) {
                fail(ec, "HttpConnectionService::close shutdown");
            }
        });
    }

    return !_quit;
}

void Ichor::HttpConnectionService::fail(beast::error_code ec, const char *what) {
    ICHOR_LOG_ERROR(_logger, "Boost.BEAST fail: {}, {}", what, ec.message());
    getManager()->pushPrioritisedEvent<StopServiceEvent>(getServiceId(), _priority, getServiceId());
}

void Ichor::HttpConnectionService::connect(tcp::endpoint endpoint, net::yield_context yield) {
    beast::error_code ec;

    tcp::resolver resolver(*_httpContext);
    _httpStream = std::make_unique<beast::tcp_stream>(*_httpContext);

    auto const results = resolver.async_resolve(endpoint, yield[ec]);
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