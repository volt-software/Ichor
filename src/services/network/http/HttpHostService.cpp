#include <ichor/DependencyManager.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/services/network/http/HttpHostService.h>
#include <ichor/services/network/tcp/TcpHostService.h>
#include <fmt/format.h>


template <>
struct fmt::formatter<Ichor::unordered_set<Ichor::ServiceIdType>> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::unordered_set<Ichor::ServiceIdType>& s, FormatContext& ctx) const {
        bool first = true;
        fmt::format_to(ctx.out(), "(");
        for(const auto& id : s) {
            if(first) {
                fmt::format_to(ctx.out(), "{}", id);
                first = false;
            } else {
                fmt::format_to(ctx.out(), ", {}", id);
            }
        }
        return fmt::format_to(ctx.out(), ")");
    }
};

Ichor::HttpHostService::HttpHostService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
    reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
    reg.registerDependency<IEventQueue>(this, DependencyFlags::REQUIRED);
    reg.registerDependency<IHostService>(this, DependencyFlags::REQUIRED, getProperties());
    reg.registerDependency<IHostConnectionService>(this, DependencyFlags::ALLOW_MULTIPLE);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::HttpHostService::start() {
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
        _priority = Ichor::any_cast<uint64_t>(propIt->second);
    }
    if(auto propIt = getProperties().find("Debug"); propIt != getProperties().end()) {
        _debug = Ichor::any_cast<bool>(propIt->second);
    }
    if(auto propIt = getProperties().find("SendServerHeader"); propIt != getProperties().end()) {
        _sendServerHeader = Ichor::any_cast<bool>(propIt->second);
    }

    co_return {};
}

Ichor::Task<void> Ichor::HttpHostService::stop() {
    co_return;
}

void Ichor::HttpHostService::addDependencyInstance(ILogger &logger, IService &) {
    _logger = &logger;
    ICHOR_LOG_TRACE(_logger, "HttpHost {} got logger", getServiceId());
}

void Ichor::HttpHostService::removeDependencyInstance(ILogger &, IService &) {
    _logger = nullptr;
}

void Ichor::HttpHostService::addDependencyInstance(IEventQueue &q, IService &) {
    ICHOR_LOG_TRACE(_logger, "HttpHost {} got queue", getServiceId());
    _queue = &q;
}

void Ichor::HttpHostService::removeDependencyInstance(IEventQueue &, IService &) {
    _queue = nullptr;
}

void Ichor::HttpHostService::addDependencyInstance(IHostService &, IService &s) {
    ICHOR_LOG_TRACE(_logger, "HttpHost {} got host service {}", getServiceId(), s.getServiceId());
    _hostServiceIds.emplace(s.getServiceId());
}

void Ichor::HttpHostService::removeDependencyInstance(IHostService &, IService &s) {
    _hostServiceIds.erase(s.getServiceId());
}

void Ichor::HttpHostService::addDependencyInstance(IHostConnectionService &client, IService &s) {
    ICHOR_LOG_TRACE(_logger, "HttpHost {} got connection {}", getServiceId(), s.getServiceId());
    if(client.isClient()) {
        ICHOR_LOG_TRACE(_logger, "connection {} is not a host connection", s.getServiceId());
        return;
    }

    auto TcpHostProp = s.getProperties().find("TcpHostService");

    if(TcpHostProp == s.getProperties().end()) {
        ICHOR_LOG_TRACE(_logger, "New connection {} did not have TcpHostService property", s.getServiceId());
        return;
    }
    if(!_hostServiceIds.contains(Ichor::any_cast<ServiceIdType>(TcpHostProp->second))) {
        ICHOR_LOG_TRACE(_logger, "New connection {}:{} did not match hostServiceId {}", s.getServiceId(), Ichor::any_cast<ServiceIdType>(TcpHostProp->second), _hostServiceIds);
        return;
    }

    client.setReceiveHandler([this, id = s.getServiceId()](std::span<uint8_t const> buffer) {
        std::string_view msg{reinterpret_cast<char const*>(buffer.data()), buffer.size()};
        auto &string = _connectionBuffers[id];
        string.append(msg.data(), msg.size());
        _queue->pushEvent<RunFunctionEventAsync>(getServiceId(), [this, id]() -> AsyncGenerator<IchorBehaviour> {
            co_await receiveRequestHandler(id);
            co_return {};
        });
    });

    _connections.emplace(s.getServiceId(), &client);
}

void Ichor::HttpHostService::removeDependencyInstance(IHostConnectionService &, IService &s) {
    _connections.erase(s.getServiceId());
    _connectionBuffers.erase(s.getServiceId());
}

tl::expected<Ichor::HttpRequest, Ichor::HttpParseError> Ichor::HttpHostService::parseRequest(std::string_view complete, size_t& len) const {
    HttpRequest req{};
    uint64_t lineNo{};
    uint64_t crlfCounter{};
    uint64_t protocolLength{};
    uint64_t contentLength{};
    std::string_view partial{complete.data(), len};
    bool badRequest{};
    bool contentLengthHeader{};
    ICHOR_LOG_TRACE(_logger, "HttpHostService {} parseResponse len {} complete \"{}\" partial \"{}\"", getServiceId(), len, complete, partial);

    split(partial, "\r\n", false, [&](std::string_view line) {
        if(badRequest) {
            return;
        }

        if(lineNo == 0) {
            uint64_t wordNo{};
            bool matchedVersion{};
            split(line, " ", false, [&](std::string_view word) {
                if(badRequest) {
                    return;
                }

                if(wordNo == 0) {
                    auto it = ICHOR_METHOD_MATCHING.find(word);
                    if(it == ICHOR_METHOD_MATCHING.end()) {
                        ICHOR_LOG_TRACE(_logger, "HttpHostService {} couldn't match method {}", getServiceId(), word);
                        badRequest = true;
                    } else {
                        req.method = it->second;
                    }
                } else if(wordNo == 1) {
                    req.route = word;
                } else if(wordNo == 2) {
                    if(word != ICHOR_HTTP_VERSION_MATCH) {
                        ICHOR_LOG_TRACE(_logger, "HttpHostService {} BadRequest version mismatch {} != {}", getServiceId(), word, ICHOR_HTTP_VERSION_MATCH);
                        badRequest = true;
                    }
                    matchedVersion = true;
                } else if(wordNo == 3) {
                    ICHOR_LOG_TRACE(_logger, "HttpHostService {} too many words on line", getServiceId(), line);
                    badRequest = true;
                }
                wordNo++;
            });

            if(!matchedVersion) {
                ICHOR_LOG_TRACE(_logger, "HttpHostService {} BadRequest did not match version", getServiceId());
                badRequest = true;
            }
            crlfCounter++;
        } else if(!line.empty()) {
            uint64_t wordNo{};
            bool matchedValue{};
            std::string_view key;
            split(line, ": ", false, [&](std::string_view word) {
                if(badRequest) {
                    return;
                }

                if(wordNo == 0) {
                    key = word;
                    if(key == "Content-Length") {
                        contentLengthHeader = true;
                    }
                } else if(wordNo == 1) {
                    req.headers.emplace(key, word);
                    matchedValue = true;
                    if(contentLengthHeader) {
                        if(!IsOnlyDigits(word)) {
                            ICHOR_LOG_TRACE(_logger, "HttpHostService {} BadRequest Content-Length not digits {}", getServiceId(), word);
                            badRequest = true;
                        } else {
                            contentLength = FastAtoiu(word);
                        }
                    }
                } else {
                    ICHOR_LOG_TRACE(_logger, "HttpHostService {} too many words on line", getServiceId(), line);
                    badRequest = true;
                }

                wordNo++;
            });

            if(!matchedValue) {
                ICHOR_LOG_TRACE(_logger, "HttpHostService {} BadRequest did not match value", getServiceId());
                badRequest = true;
            }
            crlfCounter = 1;
        } else if(line.empty()) {
            crlfCounter++;
        }

        protocolLength += line.size() + 2;
        lineNo++;
    });

    if(crlfCounter < 2) {
        ICHOR_LOG_TRACE(_logger, "HttpHostService {} not enough crlf detected {}", getServiceId(), crlfCounter);
        badRequest = true;
    }

    if(badRequest) {
        if(contentLengthHeader && complete.size() < protocolLength + contentLength) {
            len += contentLength;
        } else {
            len = partial.size();
        }
        return tl::unexpected(HttpParseError::BADREQUEST);
    }

    if(contentLengthHeader && complete.size() < protocolLength + contentLength) {
        return tl::unexpected(HttpParseError::INCOMPLETEREQUEST);
    } else {
        std::string_view content{complete.data() + len, contentLength};
        req.body.reserve(contentLength + 1);
        req.body.assign(content.begin(), content.end());
        req.body.emplace_back(0);
        len += contentLength;
    }

    ICHOR_LOG_TRACE(_logger, "HttpHostService {} parsed \"{}\" {} {} {} {}", getServiceId(), partial, ICHOR_REVERSE_METHOD_MATCHING[req.method], req.address, req.route, req.body.size());

    return req;
}

Ichor::Task<void> Ichor::HttpHostService::receiveRequestHandler(ServiceIdType id) {
    auto &string = _connectionBuffers[id];
    if(string.size() > 1024*1024*512) {
        string.clear();
        HttpResponse resp{};
        resp.status = HttpStatus::internal_server_error;
        co_await sendResponse(id, resp);
        co_return;
    }
    std::string_view msg = string;

    auto pos = msg.find("\r\n\r\n");
    ICHOR_LOG_TRACE(_logger, "HttpHostService {} pos {} total {}", getServiceId(), pos, msg.size());

    while(pos != std::string_view::npos) {
        pos += 4;
        auto req = parseRequest(msg, pos);

        HttpResponse resp{};

        if(!req) {
            if(req.error() == HttpParseError::INCOMPLETEREQUEST) {
                break;
            }

            resp.status = HttpStatus::bad_request;
        } else {
            auto routes = _handlers.find(req->method);

            if(routes == _handlers.end()) {
                resp.status = HttpStatus::not_found;
            } else {
                for(auto const &[matcher, handler] : routes->second) {
                    if(matcher->matches(req->route)) {
                        req->regex_params = matcher->route_params();

                        resp = co_await handler(*req);
                        break;
                    }
                }
            }
        }

        co_await sendResponse(id, resp);

        msg = msg.substr(pos);
        pos = msg.find("\r\n\r\n");
        ICHOR_LOG_TRACE(_logger, "HttpHostService {} new buffer {} pos {}", getServiceId(), msg, pos);
    }

    if(!msg.empty()) {
        string = msg;
    } else {
        string.clear();
    }

    co_return;
}

Ichor::Task<void> Ichor::HttpHostService::sendResponse(ServiceIdType id, const HttpResponse &response) {
    using namespace std::literals;

    std::vector<uint8_t> resp;
    resp.reserve(8192);
    auto statusText = ICHOR_STATUS_MATCHING.find(response.status);
    fmt::format_to(std::back_inserter(resp), "HTTP/1.1 {} {}\r\n", static_cast<uint_fast16_t>(response.status), statusText == ICHOR_STATUS_MATCHING.end() ? "Unknown"sv : statusText->second);
    for(auto const &[k, v] : response.headers) {
        fmt::format_to(std::back_inserter(resp), "{}: {}\r\n", k, v);
    }
    if(response.contentType) {
        fmt::format_to(std::back_inserter(resp), "Content-Type: {}\r\n", *response.contentType);
    }
    if(!response.body.empty()) {
        fmt::format_to(std::back_inserter(resp), "Content-Length: {}\r\n\r\n", response.body.size());
        resp.insert(resp.end(), response.body.begin(), response.body.end());
    } else {
        fmt::format_to(std::back_inserter(resp), "\r\n");
    }

    auto client = _connections.find(id);

    if(client == _connections.end()) {
        co_return;
    }

    co_await client->second->sendAsync(std::move(resp));
    co_return;
}

Ichor::HttpRouteRegistration Ichor::HttpHostService::addRoute(HttpMethod method, std::string_view route, std::function<Task<HttpResponse>(HttpRequest&)> handler) {
    return addRoute(method, std::make_unique<StringRouteMatcher>(route), std::move(handler));
}

Ichor::HttpRouteRegistration Ichor::HttpHostService::addRoute(HttpMethod method, std::unique_ptr<RouteMatcher> newMatcher, std::function<Task<HttpResponse>(HttpRequest&)> handler) {
    auto routes = _handlers.find(method);

    newMatcher->set_id(_matchersIdCounter);

    if(routes == _handlers.end()) {
        unordered_map<std::unique_ptr<RouteMatcher>, std::function<Task<HttpResponse>(HttpRequest&)>> newSubMap{};
        newSubMap.emplace(std::move(newMatcher), std::move(handler));
        _handlers.emplace(method, std::move(newSubMap));
    } else {
        routes->second.emplace(std::move(newMatcher), std::move(handler));
    }

    return {method, _matchersIdCounter++, this};
}

void Ichor::HttpHostService::removeRoute(HttpMethod method, RouteIdType id) {
    auto routes = _handlers.find(method);

    if(routes == std::end(_handlers)) {
        return;
    }

    std::erase_if(routes->second, [id](auto const &item) {
       return item.first->get_id() == id;
    });
}

void Ichor::HttpHostService::setPriority(uint64_t priority) {
    _priority = priority;
}

uint64_t Ichor::HttpHostService::getPriority() {
    return _priority;
}
