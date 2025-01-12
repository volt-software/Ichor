#include <ichor/services/network/http/HttpConnectionService.h>
#include <ichor/stl/StringUtils.h>

Ichor::HttpConnectionService::HttpConnectionService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
    reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
    reg.registerDependency<IEventQueue>(this, DependencyFlags::REQUIRED);
    reg.registerDependency<IClientConnectionService>(this, DependencyFlags::REQUIRED, getProperties());
}

Ichor::Task<tl::expected<Ichor::HttpResponse, Ichor::HttpError>> Ichor::HttpConnectionService::sendAsync(HttpMethod method, std::string_view route, unordered_map<std::string, std::string> &&headers, std::vector<uint8_t> &&msg) {
    if(_connection == nullptr) {
        ICHOR_LOG_TRACE(_logger, "_connection nullptr");
        co_return tl::unexpected(HttpError::NO_CONNECTION);
    }
    if(method == HttpMethod::get && !msg.empty()) {
        co_return tl::unexpected(HttpError::GET_REQUESTS_CANNOT_HAVE_BODY);
    }

    std::vector<uint8_t> resp;
    resp.reserve(8192);
    auto methodText = ICHOR_REVERSE_METHOD_MATCHING.find(method);
    if (methodText == ICHOR_REVERSE_METHOD_MATCHING.end()) {
        co_return tl::unexpected(HttpError::WRONG_METHOD);
    }
    fmt::format_to(std::back_inserter(resp), "{} {} HTTP/1.1\r\n", methodText->second, route);
    for (auto const &[k, v] : headers) {
        if(k.empty() || k.front() == ' ' || k.back() == ' ') {
        co_return tl::unexpected(HttpError::UNABLE_TO_PARSE_HEADER);
        }
        if(v.empty() || v.front() == ' ' || v.back() == ' ') {
        co_return tl::unexpected(HttpError::UNABLE_TO_PARSE_HEADER);
        }
        fmt::format_to(std::back_inserter(resp), "{}: {}\r\n", k, v);
    }
    if (headers.find("Host") == headers.end() && _address != nullptr) {
        fmt::format_to(std::back_inserter(resp), "Host: {}\r\n", *_address);
    }
    if(!msg.empty()) {
        fmt::format_to(std::back_inserter(resp), "Content-Length: {}\r\n", msg.size());
    }
    fmt::format_to(std::back_inserter(resp), "\r\n");
    if(!msg.empty()) {
        resp.insert(resp.end(), msg.begin(), msg.end());
    }
    // ICHOR_LOG_TRACE(_logger, "HttpConnection {} sending\n{}\n===", getServiceId(), std::string_view{reinterpret_cast<const char *>(resp.data()), resp.size()});
    auto success = co_await _connection->sendAsync(std::move(resp));

    if(!success) {
        co_return tl::unexpected(HttpError::IO_ERROR);
    }

    auto &evt = _events.emplace_back();

    auto &parseResp = co_await evt;

    if(!parseResp) {
        ICHOR_LOG_TRACE(_logger, "HttpConnection {} Failed to parse response: {}", getServiceId(), parseResp.error());
        co_return tl::unexpected(HttpError::UNABLE_TO_PARSE_RESPONSE);
    }

    co_return *parseResp;
}

Ichor::Task<void> Ichor::HttpConnectionService::close() {
    co_return;
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::HttpConnectionService::start() {
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

    _address = &Ichor::any_cast<std::string const &>(addrIt->second);

    if(auto propIt = getProperties().find("Priority"); propIt != getProperties().end()) {
        _priority = Ichor::any_cast<uint64_t>(propIt->second);
    }
    if(auto propIt = getProperties().find("Debug"); propIt != getProperties().end()) {
        _debug = Ichor::any_cast<bool>(propIt->second);
    }
    ICHOR_LOG_TRACE(_logger, "HttpConnection {} started", getServiceId());

    co_return {};
}

Ichor::Task<void> Ichor::HttpConnectionService::stop() {
    if(!_events.empty()) {
        std::terminate();
    }
    _address = nullptr;
    ICHOR_LOG_TRACE(_logger, "HttpConnection {} stopped", getServiceId());

    co_return;
}

void Ichor::HttpConnectionService::addDependencyInstance(ILogger &logger, IService &) {
    _logger = &logger;
    ICHOR_LOG_TRACE(_logger, "HttpConnection {} got logger", getServiceId());
}

void Ichor::HttpConnectionService::removeDependencyInstance(ILogger &, IService &) {
    _logger = nullptr;
}

void Ichor::HttpConnectionService::addDependencyInstance(IEventQueue &q, IService &) {
    _queue = &q;
    ICHOR_LOG_TRACE(_logger, "HttpConnection {} got queue", getServiceId());
}

void Ichor::HttpConnectionService::removeDependencyInstance(IEventQueue &, IService &) {
    _queue = nullptr;
}

void Ichor::HttpConnectionService::addDependencyInstance(IClientConnectionService &client, IService &s) {
    ICHOR_LOG_TRACE(_logger, "HttpConnection {} got connection {}", getServiceId(), s.getServiceId());
    if(!client.isClient()) {
        ICHOR_LOG_TRACE(_logger, "connection {} is not a client connection", s.getServiceId());
        return;
    }

    _connection = &client;
    _connection->setReceiveHandler([this](std::span<uint8_t const> buffer) {
        std::string_view msg{reinterpret_cast<char const*>(buffer.data()), buffer.size()};
        _buffer.append(msg.data(), msg.size());
        if(_buffer.size() > 1024*1024*512) {
            _buffer.clear();
            tl::expected<HttpResponse, HttpParseError> err = tl::unexpected(HttpParseError::BUFFEROVERFLOW);
            for(auto &evt : _events) {
                evt.set(err);
            }
            return;
        }
        msg = _buffer;
        // ICHOR_LOG_TRACE(_logger, "HttpConnection {} receive buffer {}", getServiceId(), msg);

        if(_events.empty()) {
            ICHOR_LOG_ERROR(_logger, "No events to set?");
        }

        auto pos = msg.find("\r\n\r\n");

        while(pos != std::string_view::npos) {
            pos += 4;
            auto resp = parseResponse(msg, pos);

            if(!_events.empty()) {
                {
                    auto &front = _events.front();
                    front.set(resp);
                }
                _events.pop_front();
            }

            msg = msg.substr(pos);
            pos = msg.find("\r\n\r\n");
        }

        if(!msg.empty()) {
            _buffer = msg;
        } else {
            _buffer.clear();
        }
    });
}

void Ichor::HttpConnectionService::removeDependencyInstance(IClientConnectionService &client, IService &) {
    if(&client != _connection) {
        return;
    }

    _connection = nullptr;
    for(auto &evt : _events) {
        evt.set(tl::unexpected(HttpParseError::QUITTING));
    }
    _events.clear();
}

void Ichor::HttpConnectionService::setPriority(uint64_t priority) {
    _priority = priority;
}

uint64_t Ichor::HttpConnectionService::getPriority() {
    return _priority;
}

tl::expected<Ichor::HttpResponse, Ichor::HttpParseError> Ichor::HttpConnectionService::parseResponse(std::string_view complete, size_t& len) const {
    HttpResponse resp{};
    uint64_t lineNo{};
    uint64_t crlfCounter{};
    uint64_t contentLength{};
    std::string_view partial{complete.data(), len};
    bool badRequest{};
    // ICHOR_LOG_TRACE(_logger, "HttpConnection {} parseResponse {}", getServiceId(), complete);

    split(partial, "\r\n", false, [&](std::string_view line) {
        if(badRequest) {
            return;
        }

        if(lineNo == 0) {
            uint64_t wordNo{};
            bool matchedStatus{};
            split(line, " ", false, [&](std::string_view word) {
                if(badRequest) {
                    return;
                }

                if(wordNo == 0) {
                    if(word != ICHOR_HTTP_VERSION_MATCH) {
                        ICHOR_LOG_TRACE(_logger, "HttpConnection {} BadRequest version mismatch {} != {}", getServiceId(), word, ICHOR_HTTP_VERSION_MATCH);
                        badRequest = true;
                    }
                } else if(wordNo == 1) {
                    if(word.empty() || !std::ranges::all_of(word, ::isdigit)) {
                        ICHOR_LOG_TRACE(_logger, "HttpConnection {} BadRequest version !all_of digits {}", getServiceId(), word);
                        badRequest = true;
                    } else {
                        auto status = FastAtoiu(word.data());
                        if(status < 100 || status > 599) {
                            ICHOR_LOG_TRACE(_logger, "HttpConnection {} BadRequest HTTP status out of range {}", getServiceId(), status);
                            badRequest = true;
                        } else {
                            resp.status = static_cast<HttpStatus>(status);
                            matchedStatus = true;
                        }
                    }
                }
                wordNo++;
            });

            if(!matchedStatus) {
                ICHOR_LOG_TRACE(_logger, "HttpConnection {} BadRequest did not match status", getServiceId());
                badRequest = true;
            }
            crlfCounter++;
        } else if(!line.empty()) {
            uint64_t wordNo{};
            bool matchedValue{};
            bool contentLengthHeader{};
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
                    resp.headers.emplace(key, word);
                    matchedValue = true;
                    if(contentLengthHeader) {
                        if(!IsOnlyDigits(word)) {
                            ICHOR_LOG_TRACE(_logger, "HttpConnection {} BadRequest Content-Length not digits {}", getServiceId(), word);
                            badRequest = true;
                        } else {
                            contentLength = FastAtoiu(word);
                        }
                    }
                } else {
                    ICHOR_LOG_TRACE(_logger, "HttpConnection {} BadRequest header split error {}", getServiceId(), line);
                    badRequest = true;
                }

                wordNo++;
            });

            if(!matchedValue) {
                ICHOR_LOG_TRACE(_logger, "HttpConnection {} BadRequest header did not match value {}", getServiceId(), line);
                badRequest = true;
            }
            crlfCounter = 1;
        } else if(line.empty()) {
            crlfCounter++;
        }

        lineNo++;
    });

    if(crlfCounter < 2) {
        ICHOR_LOG_TRACE(_logger, "HttpConnection {} not enough crlf detected {}", getServiceId(), crlfCounter);
        badRequest = true;
    }

    if(badRequest) {
        len = complete.size();
        return tl::unexpected(HttpParseError::BADREQUEST);
    }

    if(complete.size() - len != contentLength) {
        ICHOR_LOG_TRACE(_logger, "HttpConnection {} BadRequest complete.size() - len != contentLength ({} - {} != {})", getServiceId(), complete.size(), len, contentLength);
        ICHOR_LOG_TRACE(_logger, "HttpConnection {} BadRequest complete message: {}", getServiceId(), complete);
        ICHOR_LOG_TRACE(_logger, "HttpConnection {} BadRequest partial message: {}", getServiceId(), partial);
        badRequest = true;
    } else {
        std::string_view content{complete.data() + len, contentLength};
        resp.body.reserve(contentLength + 1);
        resp.body.assign(content.begin(), content.end());
        resp.body.emplace_back(0);
        len = complete.size();
    }

    if(badRequest) {
        len = complete.size();
        return tl::unexpected(HttpParseError::BADREQUEST);
    }

    ICHOR_LOG_TRACE(_logger, "HttpConnection {} parsed \"{}\" {} {}", getServiceId(), partial, static_cast<uint_fast16_t>(resp.status), resp.body.size());

    return resp;
}
