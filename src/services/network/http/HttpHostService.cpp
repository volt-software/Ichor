#include <ichor/DependencyManager.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/services/network/http/HttpHostService.h>
#include <ichor/services/network/tcp/TcpHostService.h>
#include <fmt/format.h>
#include <ichor/ScopedServiceProxy.h>
#include <algorithm>
#include <limits>
#include <ichor/events/InternalEvents.h>


template <>
struct fmt::formatter<Ichor::unordered_set<Ichor::ServiceIdType, Ichor::ServiceIdHash>> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::unordered_set<Ichor::ServiceIdType, Ichor::ServiceIdHash>& s, FormatContext& ctx) const {
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

namespace {
    using namespace Ichor::v1;

    constexpr std::string_view CONTINUE_RESPONSE = "HTTP/1.1 100 Continue\r\n\r\n";

    /// Finds a field in a response's headers, ignoring the case of the name
    template <typename Headers>
    auto findField(Headers &headers, std::string_view name) {
        return std::ranges::find_if(headers, [name](auto const &field) {
            return HttpHeaderNameEqual{}(field.first, name);
        });
    }

    std::string_view trimOws(std::string_view str) noexcept {
        while(!str.empty() && (str.front() == ' ' || str.front() == '\t')) {
            str.remove_prefix(1);
        }
        while(!str.empty() && (str.back() == ' ' || str.back() == '\t')) {
            str.remove_suffix(1);
        }
        return str;
    }

    struct EntityTag {
        bool weak{};
        /// With the quotes
        std::string_view opaqueTag;
    };

    /// entity-tag = [ "W/" ] DQUOTE *etagc DQUOTE (RFC 9110 8.8.3)
    tl::optional<EntityTag> parseEntityTag(std::string_view str) noexcept {
        EntityTag tag;
        str = trimOws(str);
        if(str.starts_with("W/")) {
            tag.weak = true;
            str.remove_prefix(2);
        }
        if(str.size() < 2 || str.front() != '"' || str.substr(1).find('"') != str.size() - 2) {
            return tl::nullopt;
        }
        tag.opaqueTag = str;
        return tag;
    }

    /// Whether a list of entity tags (If-Match, If-None-Match) contains one that matches etag. The list can't be split on ',', which is allowed in a tag.
    bool entityTagListMatches(std::string_view list, EntityTag const &etag, bool strong) noexcept {
        size_t i{};
        while(i < list.size()) {
            while(i < list.size() && (list[i] == ' ' || list[i] == '\t' || list[i] == ',')) {
                i++;
            }
            if(i >= list.size()) {
                break;
            }
            auto const quoteStart = list.find('"', i);
            auto const quoteEnd = quoteStart == std::string_view::npos ? std::string_view::npos : list.find('"', quoteStart + 1);
            if(quoteEnd == std::string_view::npos) {
                return false;
            }
            auto const tag = parseEntityTag(list.substr(i, quoteEnd + 1 - i));
            if(!tag) {
                return false;
            }
            // Strong comparison: both strong and the same, weak comparison: the same opaque tags (RFC 9110 8.8.3.2)
            if(tag->opaqueTag == etag.opaqueTag && (!strong || (!tag->weak && !etag.weak))) {
                return true;
            }
            i = quoteEnd + 1;
        }
        return false;
    }

    /// Evaluates the preconditions of a GET or HEAD request against its 2xx response, in the order of RFC 9110 13.2.2
    tl::optional<HttpStatus> evaluatePreconditions(HttpHeaders const &request, Ichor::unordered_map<std::string, std::string> const &response) {
        auto const etagIt = findField(response, "ETag");
        auto const etag = etagIt == response.end() ? tl::nullopt : parseEntityTag(etagIt->second);
        auto const lastModifiedIt = findField(response, "Last-Modified");
        auto const lastModified = lastModifiedIt == response.end() ? tl::nullopt : Ichor::v1::Detail::parseHttpDate(lastModifiedIt->second);

        auto const ifMatch = request.find("If-Match");
        if(ifMatch != request.end()) {
            if(trimOws(ifMatch->second) != "*" && (!etag || !entityTagListMatches(ifMatch->second, *etag, true))) {
                return HttpStatus::precondition_failed;
            }
        } else if(auto const ifUnmodifiedSince = request.find("If-Unmodified-Since"); ifUnmodifiedSince != request.end() && lastModified) {
            // An invalid date is ignored
            auto const date = Ichor::v1::Detail::parseHttpDate(ifUnmodifiedSince->second);
            if(date && *lastModified > *date) {
                return HttpStatus::precondition_failed;
            }
        }

        auto const ifNoneMatch = request.find("If-None-Match");
        if(ifNoneMatch != request.end()) {
            if(trimOws(ifNoneMatch->second) == "*" || (etag && entityTagListMatches(ifNoneMatch->second, *etag, false))) {
                return HttpStatus::not_modified;
            }
        } else if(auto const ifModifiedSince = request.find("If-Modified-Since"); ifModifiedSince != request.end() && lastModified) {
            auto const date = Ichor::v1::Detail::parseHttpDate(ifModifiedSince->second);
            if(date && *lastModified <= *date) {
                return HttpStatus::not_modified;
            }
        }

        return tl::nullopt;
    }

    /// Without operator[], which would add unknown methods to the map that other threads read
    std::string_view methodName(HttpMethod method) noexcept {
        auto const name = ICHOR_REVERSE_METHOD_MATCHING.find(method);
        return name == ICHOR_REVERSE_METHOD_MATCHING.end() ? std::string_view{"unknown"} : name->second;
    }

    std::string joinMethods(std::vector<HttpMethod> const &methods) {
        std::string ret;
        for(auto const method : methods) {
            if(!ret.empty()) {
                ret += ", ";
            }
            ret += methodName(method);
        }
        return ret;
    }

    /// Huge timeouts would overflow the deadlines, a century is as good as never
    std::chrono::milliseconds timeoutFromProperty(uint64_t ms) noexcept {
        constexpr uint64_t century = 100ULL * 366 * 24 * 60 * 60 * 1'000;
        return std::chrono::milliseconds{static_cast<std::chrono::milliseconds::rep>(std::min(ms, century))};
    }
}

namespace Ichor::v1::Detail {
    /// The connection after a 101 Switching Protocols or a 2xx response to CONNECT
    class HttpHostUpgradedConnection final : public IHttpUpgradedConnection {
    public:
        HttpHostUpgradedConnection(HttpHostService &host, ServiceIdType id) noexcept : _host(&host), _id(id) {}
        ~HttpHostUpgradedConnection() final = default;

        Task<tl::expected<void, IOError>> sendAsync(std::vector<uint8_t> &&msg) final {
            if(_host == nullptr || !_open) {
                co_return tl::unexpected(IOError::NOT_CONNECTED);
            }
            if(!co_await _host->send(_id, std::move(msg))) {
                co_return tl::unexpected(IOError::NOT_CONNECTED);
            }
            co_return {};
        }

        void setReceiveHandler(std::function<void(std::span<uint8_t const>)> handler) final {
            // Kept after the connection is gone, a handler that holds on to this connection would never be destroyed
            if(_host == nullptr) {
                return;
            }
            _handler = std::move(handler);
            if(!_pending.empty() && _handler) {
                auto pending = std::move(_pending);
                _pending.clear();
                callHandler(pending);
            }
        }

        void close() final {
            if(_host != nullptr && _open) {
                _open = false;
                _host->closeConnection(_id);
            }
        }

        [[nodiscard]] bool isOpen() const noexcept final {
            return _host != nullptr && _open;
        }

        /// What the connection received
        void deliver(std::span<uint8_t const> data) {
            if(_handler) {
                callHandler(data);
            } else {
                _pending.insert(_pending.end(), data.begin(), data.end());
            }
        }

        /// The connection is gone, or the host is. The handler is destroyed, it can hold on to this connection.
        void detach() noexcept {
            _open = false;
            _host = nullptr;
            _pending.clear();
            // Not while it runs, e.g. when it closes the connection
            if(!_calling) {
                _handler = {};
            }
        }

    private:
        void callHandler(std::span<uint8_t const> data) {
            _calling = true;
            _handler(data);
            _calling = false;
            if(_host == nullptr) {
                _handler = {};
            }
        }

        HttpHostService *_host;
        ServiceIdType _id;
        bool _open{true};
        bool _calling{};
        std::function<void(std::span<uint8_t const>)> _handler;
        std::vector<uint8_t> _pending;
    };

    /// Sends the response of a streaming route
    class HttpHostResponseWriter final : public IHttpResponseWriter {
    public:
        HttpHostResponseWriter(HttpHostService &host, ServiceIdType id, HttpHostService::ResponseContext const &context, bool keepAlive) noexcept :
                _host(host), _id(id), _context(context), _keepAlive(keepAlive) {}

        Task<tl::expected<void, IOError>> start(HttpResponse response) final {
            if(_started) {
                co_return tl::unexpected(IOError::FAILED);
            }
            _started = true;
            auto serialized = _host.serializeResponse(response, _context, true, !_keepAlive);
            _close = serialized.close;
            _framing = serialized.framing;
            _replaced = serialized.replaced;
            _status = response.status;
            if(!response.body.empty()) {
                appendContent(serialized.msg, response.body);
            }
            if(!co_await _host.send(_id, std::move(serialized.msg))) {
                _failed = true;
                co_return tl::unexpected(IOError::NOT_CONNECTED);
            }
            if(_replaced) {
                co_return tl::unexpected(IOError::FAILED);
            }
            co_return {};
        }

        Task<tl::expected<void, IOError>> write(std::vector<uint8_t> data) final {
            if(!_started) {
                // Named: gcc 12 destroys an aggregate temporary in a co_await expression twice
                HttpResponse ok{};
                auto started = co_await start(std::move(ok));
                if(!started) {
                    co_return started;
                }
            }
            if(_failed || _upgraded) {
                co_return tl::unexpected(IOError::NOT_CONNECTED);
            }
            // The content of the response that was replaced by a 500
            if(_replaced) {
                co_return tl::unexpected(IOError::FAILED);
            }
            // A streamed response with a Content-Length is a 205, which has no content
            if(data.empty() || _framing == HttpHostService::ResponseFraming::NONE || _framing == HttpHostService::ResponseFraming::CONTENT_LENGTH) {
                co_return {};
            }
            std::vector<uint8_t> msg;
            appendContent(msg, data);
            if(!co_await _host.send(_id, std::move(msg))) {
                _failed = true;
                co_return tl::unexpected(IOError::NOT_CONNECTED);
            }
            co_return {};
        }

        [[nodiscard]] tl::optional<std::shared_ptr<IHttpUpgradedConnection>> upgrade() final {
            if(!_started || _upgraded || _failed || !switchesProtocols()) {
                return tl::nullopt;
            }
            auto state = _host._connectionStates.find(_id);
            if(state == _host._connectionStates.end() || state->second.closing) {
                return tl::nullopt;
            }
            auto connection = std::make_shared<HttpHostUpgradedConnection>(_host, _id);
            // What the client sent after the request is the new protocol's
            if(auto const pending = state->second.pending(); !pending.empty()) {
                connection->deliver(std::span<uint8_t const>{reinterpret_cast<uint8_t const*>(pending.data()), pending.size()});
                state->second.clearBuffer();
            }
            state->second.phase = HttpHostService::ConnectionPhase::UPGRADED;
            state->second.upgraded = connection;
            _upgraded = true;
            return connection;
        }

        /// Ends the content after the handler is done, returns whether the connection has to be closed
        Task<bool> finish() {
            if(!_started) {
                HttpResponse ok{};
                if(!co_await start(std::move(ok))) {
                    co_return true;
                }
            }
            if(_failed) {
                co_return true;
            }
            if(_upgraded) {
                co_return false;
            }
            // Switched protocols, but nobody took the connection over
            if(switchesProtocols()) {
                co_return true;
            }
            if(_framing == HttpHostService::ResponseFraming::CHUNKED) {
                // last-chunk and an empty trailer section (RFC 9112 7.1)
                constexpr std::string_view lastChunk = "0\r\n\r\n";
                if(!co_await _host.send(_id, std::vector<uint8_t>{lastChunk.begin(), lastChunk.end()})) {
                    co_return true;
                }
            }
            co_return _close;
        }

    private:
        [[nodiscard]] bool switchesProtocols() const noexcept {
            auto const status = static_cast<uint_fast16_t>(_status);
            return status == 101 || (_context.method == HttpMethod::connect && status >= 200 && status < 300);
        }

        void appendContent(std::vector<uint8_t> &msg, std::span<uint8_t const> data) const {
            if(_framing == HttpHostService::ResponseFraming::NONE || _framing == HttpHostService::ResponseFraming::CONTENT_LENGTH) {
                return;
            }
            if(_framing == HttpHostService::ResponseFraming::CHUNKED) {
                // chunk = chunk-size CRLF chunk-data CRLF (RFC 9112 7.1)
                fmt::format_to(FmtU8Inserter(msg), "{:x}\r\n", data.size());
                msg.insert(msg.end(), data.begin(), data.end());
                msg.push_back('\r');
                msg.push_back('\n');
            } else {
                msg.insert(msg.end(), data.begin(), data.end());
            }
        }

        HttpHostService &_host;
        ServiceIdType _id;
        HttpHostService::ResponseContext const &_context;
        bool _keepAlive;
        bool _started{};
        bool _failed{};
        bool _upgraded{};
        bool _replaced{};
        bool _close{};
        HttpStatus _status{HttpStatus::ok};
        HttpHostService::ResponseFraming _framing{HttpHostService::ResponseFraming::NONE};
    };
}

Ichor::v1::HttpHostService::HttpHostService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
    reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
    reg.registerDependency<IEventQueue>(this, DependencyFlags::REQUIRED);
    reg.registerDependency<IHostService>(this, DependencyFlags::REQUIRED, getProperties());
    reg.registerDependency<IHostConnectionService>(this, DependencyFlags::ALLOW_MULTIPLE);
    // Optional: without one there are no timeouts and connections are closed right away
    reg.registerDependency<ITimerFactory>(this, DependencyFlags::NONE);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::v1::HttpHostService::start() {
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
    if(auto propIt = getProperties().find("EnableTrace"); propIt != getProperties().end()) {
        _enableTrace = Ichor::v1::any_cast<bool>(propIt->second);
    }
    if(auto propIt = getProperties().find("MaxRequestLineSize"); propIt != getProperties().end()) {
        _limits.maxRequestLineSize = Ichor::v1::any_cast<uint64_t>(propIt->second);
    }
    if(auto propIt = getProperties().find("MaxHeaderSize"); propIt != getProperties().end()) {
        _limits.maxHeaderSectionSize = Ichor::v1::any_cast<uint64_t>(propIt->second);
    }
    if(auto propIt = getProperties().find("MaxBodySize"); propIt != getProperties().end()) {
        _limits.maxBodySize = Ichor::v1::any_cast<uint64_t>(propIt->second);
    }
    if(auto propIt = getProperties().find("IdleTimeoutMs"); propIt != getProperties().end()) {
        _idleTimeout = timeoutFromProperty(Ichor::v1::any_cast<uint64_t>(propIt->second));
    }
    if(auto propIt = getProperties().find("RequestTimeoutMs"); propIt != getProperties().end()) {
        _requestTimeout = timeoutFromProperty(Ichor::v1::any_cast<uint64_t>(propIt->second));
    }
    if(auto propIt = getProperties().find("LingerTimeoutMs"); propIt != getProperties().end()) {
        _lingerTimeout = timeoutFromProperty(Ichor::v1::any_cast<uint64_t>(propIt->second));
    }

    // Connections that came before start() got the default timeouts
    for(auto &[id, state] : _connectionStates) {
        if(state.phase == ConnectionPhase::IDLE || state.phase == ConnectionPhase::REQUEST) {
            setIdleOrRequestPhase(state);
        }
    }

    _started = true;
    startTimeoutTimer();

    co_return {};
}

Ichor::Task<void> Ichor::v1::HttpHostService::stop() {
    _started = false;
    stopTimeoutTimer();
    for(auto &[id, state] : _connectionStates) {
        if(state.upgraded) {
            state.upgraded->detach();
            state.upgraded.reset();
        }
    }
    co_return;
}

void Ichor::v1::HttpHostService::addDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService &) {
    _logger = std::move(logger);
    ICHOR_LOG_TRACE(_logger, "HttpHost {} got logger", getServiceId());
}

void Ichor::v1::HttpHostService::removeDependencyInstance(Ichor::ScopedServiceProxy<ILogger*>, IService &) {
    _logger = nullptr;
}

void Ichor::v1::HttpHostService::addDependencyInstance(Ichor::ScopedServiceProxy<IEventQueue*> q, IService &) {
    ICHOR_LOG_TRACE(_logger, "HttpHost {} got queue", getServiceId());
    _queue = std::move(q);
}

void Ichor::v1::HttpHostService::removeDependencyInstance(Ichor::ScopedServiceProxy<IEventQueue*>, IService &) {
    _queue = nullptr;
}

void Ichor::v1::HttpHostService::addDependencyInstance(Ichor::ScopedServiceProxy<IHostService*>, IService &s) {
    ICHOR_LOG_TRACE(_logger, "HttpHost {} got host service {}", getServiceId(), s.getServiceId());
    _hostServiceIds.emplace(s.getServiceId());
}

void Ichor::v1::HttpHostService::removeDependencyInstance(Ichor::ScopedServiceProxy<IHostService*>, IService &s) {
    _hostServiceIds.erase(s.getServiceId());
}

void Ichor::v1::HttpHostService::addDependencyInstance(Ichor::ScopedServiceProxy<ITimerFactory*> timerFactory, IService &) {
    _timerFactory = std::move(timerFactory);
    if(_started) {
        startTimeoutTimer();
    }
}

void Ichor::v1::HttpHostService::removeDependencyInstance(Ichor::ScopedServiceProxy<ITimerFactory*>, IService &) {
    // The factory has destroyed its timers already
    _timeoutTimer.reset();
    _timerFactory = nullptr;
    // Nothing would end their lingering anymore
    std::vector<ServiceIdType> lingering;
    for(auto const &[id, state] : _connectionStates) {
        if(state.phase == ConnectionPhase::LINGERING) {
            lingering.push_back(id);
        }
    }
    for(auto const id : lingering) {
        stopConnection(id);
    }
}

void Ichor::v1::HttpHostService::addDependencyInstance(Ichor::ScopedServiceProxy<IHostConnectionService*> client, IService &s) {
    ICHOR_LOG_TRACE(_logger, "HttpHost {} got connection {}", getServiceId(), s.getServiceId());
    if(client->isClient()) {
        ICHOR_LOG_TRACE(_logger, "connection {} is not a host connection", s.getServiceId());
        return;
    }

    auto TcpHostProp = s.getProperties().find("TcpHostService");

    if(TcpHostProp == s.getProperties().end()) {
        ICHOR_LOG_TRACE(_logger, "New connection {} did not have TcpHostService property", s.getServiceId());
        return;
    }
    if(!_hostServiceIds.contains(Ichor::v1::any_cast<ServiceIdType>(TcpHostProp->second))) {
        ICHOR_LOG_TRACE(_logger, "New connection {}:{} did not match hostServiceId {}", s.getServiceId(), Ichor::v1::any_cast<ServiceIdType>(TcpHostProp->second), _hostServiceIds);
        return;
    }

    // Before setting the receive handler, which can be called right away with what was already received
    _connections.emplace(s.getServiceId(), client);
    ConnectionState state{};
    setIdleOrRequestPhase(state);
    _connectionStates.emplace(s.getServiceId(), std::move(state));

    client->setReceiveHandler([this, id = s.getServiceId()](std::span<uint8_t const> buffer) {
        receive(id, buffer);
    });
}

void Ichor::v1::HttpHostService::removeDependencyInstance(Ichor::ScopedServiceProxy<IHostConnectionService*>, IService &s) {
    if(auto state = _connectionStates.find(s.getServiceId()); state != _connectionStates.end() && state->second.upgraded) {
        state->second.upgraded->detach();
    }
    _connections.erase(s.getServiceId());
    _connectionStates.erase(s.getServiceId());
}

void Ichor::v1::HttpHostService::receive(ServiceIdType id, std::span<uint8_t const> data) {
    auto stateIt = _connectionStates.find(id);
    if(stateIt == _connectionStates.end()) {
        return;
    }
    auto &state = stateIt->second;
    if(state.phase == ConnectionPhase::UPGRADED) {
        // A copy: the handler can close the connection
        if(auto upgraded = state.upgraded) {
            upgraded->deliver(data);
        }
        return;
    }
    if(state.closing) {
        return;
    }
    state.buffer.append(reinterpret_cast<char const*>(data.data()), data.size());
    // Requests pipelined while one is being handled can't be answered with 413 or 431 before it is done, but there's no reason to keep receiving
    // more than one request's worth
    auto const requestSize = _limits.maxHeaderSectionSize > std::numeric_limits<uint64_t>::max() - _limits.maxBodySize ? std::numeric_limits<uint64_t>::max() : _limits.maxHeaderSectionSize + _limits.maxBodySize;
    if(state.processing && state.pending().size() > requestSize) {
        ICHOR_LOG_TRACE(_logger, "HttpHostService {} connection {} pipelined more than a request's worth, closing", getServiceId(), id);
        stopConnection(id);
        return;
    }
    // The request timeout starts with its first byte, not its last (slow loris)
    if(state.phase == ConnectionPhase::IDLE) {
        setIdleOrRequestPhase(state);
    }
    _queue->pushPrioritisedEvent<RunFunctionEventAsync>(getServiceId(), _priority, [this, id]() -> AsyncGenerator<IchorBehaviour> {
        co_await receiveRequestHandler(id);
        co_return {};
    });
}

Ichor::Task<void> Ichor::v1::HttpHostService::receiveRequestHandler(ServiceIdType id) {
    {
        // Requests are handled one at a time: one that arrives while another is being handled waits in the buffer (RFC 9112 9.3.2)
        auto state = _connectionStates.find(id);
        if(state == _connectionStates.end() || state->second.processing) {
            co_return;
        }
        state->second.processing = true;
    }

    while(true) {
        // Looked up again after every co_await, the connection might be gone
        auto stateIt = _connectionStates.find(id);
        if(stateIt == _connectionStates.end()) {
            co_return;
        }
        auto &state = stateIt->second;
        if(state.closing || state.phase == ConnectionPhase::UPGRADED) {
            break;
        }

        // Continues where the last parse of this request left off: a request received in many parts isn't parsed again for each of them
        auto result = Detail::parseHttpRequest(state.pending(), _limits, state.parse);

        if(result.status == Detail::HttpRequestParseStatus::INCOMPLETE) {
            if(result.expectContinue && !state.sentContinue) {
                // The client waits for this before it sends the body (RFC 9110 10.1.1)
                state.sentContinue = true;
                if(!co_await send(id, std::vector<uint8_t>{CONTINUE_RESPONSE.begin(), CONTINUE_RESPONSE.end()})) {
                    co_return;
                }
                continue;
            }
            break;
        }

        if(result.status == Detail::HttpRequestParseStatus::INVALID) {
            ICHOR_LOG_TRACE(_logger, "HttpHostService {} connection {} invalid request, responding with {}", getServiceId(), id, static_cast<uint_fast16_t>(result.errorStatus));
            state.closing = true;
            state.clearBuffer();
            HttpResponse response{.status = result.errorStatus, .contentType = {}, .body = {}, .headers = {}};
            co_await sendResponse(id, response, ResponseContext{}, false);
            closeConnection(id);
            break;
        }

        state.consume(static_cast<size_t>(result.consumed));
        bool const sentContinue = state.sentContinue;
        state.sentContinue = false;
        state.phase = ConnectionPhase::PROCESSING;
        auto request = std::move(result.request);
        ICHOR_LOG_TRACE(_logger, "HttpHostService {} connection {} request {} {}", getServiceId(), id, methodName(request.method), request.target);

        bool const close = co_await handleRequest(id, request, sentContinue);

        auto after = _connectionStates.find(id);
        if(after == _connectionStates.end()) {
            co_return;
        }
        // Upgraded, or closed by the handler through the upgraded connection: lingering, which a new phase would undo
        if(after->second.phase == ConnectionPhase::UPGRADED || after->second.closing) {
            break;
        }
        if(close) {
            closeConnection(id);
            break;
        }
        setIdleOrRequestPhase(after->second);
    }

    if(auto state = _connectionStates.find(id); state != _connectionStates.end()) {
        state->second.processing = false;
    }
    co_return;
}

Ichor::Task<bool> Ichor::v1::HttpHostService::handleRequest(ServiceIdType id, Detail::ParsedHttpRequest &request, bool sentContinue) {
    ResponseContext context{.method = request.method, .target = request.target, .minorVersion = request.minorVersion, .requestHeaders = &request.headers,
                            .owesContinue = request.expectContinue && !sentContinue};

    if(request.method == HttpMethod::unknown) {
        HttpResponse response{.status = HttpStatus::not_implemented, .contentType = {}, .body = {}, .headers = {}};
        co_return co_await sendResponse(id, response, context, request.keepAlive);
    }

    // OPTIONS * is about the server, not a resource (RFC 9110 9.3.7)
    if(request.target == "*") {
        std::vector<HttpMethod> methods;
        for(auto const &[method, routes] : _handlers) {
            if(!routes.empty()) {
                methods.push_back(method);
            }
        }
        methods.push_back(HttpMethod::get);
        methods.push_back(HttpMethod::head);
        methods.push_back(HttpMethod::options);
        std::ranges::sort(methods);
        auto const duplicates = std::ranges::unique(methods);
        methods.erase(duplicates.begin(), duplicates.end());
        HttpResponse response{.status = HttpStatus::ok, .contentType = {}, .body = {}, .headers = {{"Allow", joinMethods(methods)}}};
        co_return co_await sendResponse(id, response, context, request.keepAlive);
    }

    auto findRoute = [this, &request](HttpMethod method) -> std::pair<RouteMatcher*, RouteHandler*> {
        auto routes = _handlers.find(method);
        if(routes == _handlers.end()) {
            return {};
        }
        for(auto &[matcher, handler] : routes->second) {
            if(matcher->matches(request.target)) {
                return {matcher.get(), &handler};
            }
        }
        return {};
    };

    auto route = findRoute(request.method);
    // HEAD is GET without the content (RFC 9110 9.3.2)
    if(route.first == nullptr && request.method == HttpMethod::head) {
        route = findRoute(HttpMethod::get);
    }

    // Reflect the request, without what's likely sensitive (RFC 9110 9.3.8)
    if(route.first == nullptr && request.method == HttpMethod::trace && _enableTrace) {
        std::string reflected;
        split(request.head, "\r\n", true, [&reflected](std::string_view line) {
            auto const name = line.substr(0, line.find(':'));
            if(HttpHeaderNameEqual{}(name, "Authorization") || HttpHeaderNameEqual{}(name, "Proxy-Authorization") || HttpHeaderNameEqual{}(name, "Cookie")) {
                return;
            }
            reflected += line;
        });
        HttpResponse response{.status = HttpStatus::ok, .contentType = "message/http", .body = {reflected.begin(), reflected.end()}, .headers = {}};
        co_return co_await sendResponse(id, response, context, request.keepAlive);
    }

    if(route.first == nullptr) {
        auto methods = allowedMethods(request.target);
        HttpResponse response{.status = HttpStatus::not_found, .contentType = {}, .body = {}, .headers = {}};
        if(!methods.empty()) {
            // 405 has to list the allowed methods (RFC 9110 15.5.6), as does a response to OPTIONS
            response.status = request.method == HttpMethod::options ? HttpStatus::ok : HttpStatus::method_not_allowed;
            response.headers.emplace("Allow", joinMethods(methods));
        }
        co_return co_await sendResponse(id, response, context, request.keepAlive);
    }

    // The request owns what the views in HttpRequest refer to
    std::vector<uint8_t> body = std::move(request.body);
    // Some parsers expect a null terminator (rapidjson), it's not part of the content
    body.push_back(0);
    HttpRequest httpRequest{.body = std::move(body), .method = request.method, .route = request.target, .regex_params = route.first->route_params(), .address = {}, .headers = std::move(request.headers)};
    context.requestHeaders = &httpRequest.headers;

    if(route.second->streamingHandler) {
        // A copy, the route could be removed while the handler runs
        auto handler = route.second->streamingHandler;
        Detail::HttpHostResponseWriter writer{*this, id, context, request.keepAlive};
        co_await handler(httpRequest, writer);
        co_return co_await writer.finish();
    }

    auto handler = route.second->handler;
    auto response = co_await handler(httpRequest);

    auto const status = static_cast<uint_fast16_t>(response.status);
    if((request.method == HttpMethod::get || request.method == HttpMethod::head) && status >= 200 && status < 300) {
        if(auto precondition = evaluatePreconditions(httpRequest.headers, response.headers)) {
            response.status = *precondition;
            // A Content-Length the handler set for content it did send isn't the host's to pass on, only a HEAD route's (RFC 9110 8.6)
            if(!response.body.empty()) {
                std::erase_if(response.headers, [](auto const &field) {
                    return HttpHeaderNameEqual{}(field.first, "Content-Length");
                });
            }
            response.body.clear();
            if(*precondition == HttpStatus::precondition_failed) {
                response.contentType.reset();
            }
        }
    }

    co_return co_await sendResponse(id, response, context, request.keepAlive);
}

std::vector<Ichor::v1::HttpMethod> Ichor::v1::HttpHostService::allowedMethods(std::string_view route) {
    std::vector<HttpMethod> methods;
    for(auto const &[method, routes] : _handlers) {
        if(std::ranges::any_of(routes, [route](auto const &matcherAndHandler) { return matcherAndHandler.first->matches(route); })) {
            methods.push_back(method);
        }
    }
    if(methods.empty()) {
        return methods;
    }
    if(std::ranges::find(methods, HttpMethod::get) != methods.end()) {
        methods.push_back(HttpMethod::head);
    }
    methods.push_back(HttpMethod::options);
    std::ranges::sort(methods);
    auto const duplicates = std::ranges::unique(methods);
    methods.erase(duplicates.begin(), duplicates.end());
    return methods;
}

bool Ichor::v1::HttpHostService::canSwitchProtocols(HttpResponse const &response, ResponseContext const &context) const {
    // Upgrade is ignored in HTTP/1.0, and has to be a connection option (RFC 9110 7.6.1, 7.8)
    if(context.requestHeaders == nullptr || context.minorVersion == 0) {
        return false;
    }
    auto const requested = context.requestHeaders->find("Upgrade");
    auto const connection = context.requestHeaders->find("Connection");
    auto const switchTo = findField(response.headers, "Upgrade");
    if(requested == context.requestHeaders->end() || connection == context.requestHeaders->end() || switchTo == response.headers.end() ||
       !Detail::httpListContains(connection->second, "upgrade")) {
        return false;
    }
    // Only to protocols the client asked for
    bool valid{true};
    bool any{};
    split(switchTo->second, ",", false, [&](std::string_view protocol) {
        protocol = trimOws(protocol);
        if(protocol.empty()) {
            return;
        }
        any = true;
        if(!Detail::httpListContains(requested->second, protocol)) {
            valid = false;
        }
    });
    return valid && any;
}

Ichor::v1::HttpHostService::SerializedResponse Ichor::v1::HttpHostService::serializeResponse(HttpResponse &response, ResponseContext const &context, bool streaming, bool close) {
    using namespace std::literals;
    ResponseFraming framing{};

    auto status = static_cast<uint_fast16_t>(response.status);
    // A field that isn't a valid field line would split the response (RFC 9110 5.5, RFC 9112 5). An interim response only works for switching
    // protocols, which a streaming route can take over.
    bool const valid = status >= 100 && status <= 599 &&
            (status >= 200 || (status == 101 && streaming && canSwitchProtocols(response, context))) &&
            (!response.contentType || Detail::isHttpFieldValue(*response.contentType)) &&
            std::ranges::all_of(response.headers, [](auto const &field) {
                return Detail::isHttpToken(field.first) && Detail::isHttpFieldValue(field.second);
            });
    if(!valid) {
        ICHOR_LOG_ERROR(_logger, "HttpHostService {} handler responded with an invalid status {} or header, responding with 500 instead", getServiceId(), status);
        response = HttpResponse{.status = HttpStatus::internal_server_error, .contentType = {}, .body = {}, .headers = {}};
        status = 500;
    }
    bool const replaced = !valid;

    // A 405 has to say what is allowed (RFC 9110 15.5.6), which the host knows
    if(status == 405 && findField(response.headers, "Allow") == response.headers.end()) {
        auto methods = allowedMethods(context.target);
        std::erase(methods, context.method);
        response.headers.emplace("Allow", joinMethods(methods));
    }
    // Fields these have to have, which only the handler can know (RFC 9110 11.6.1, 11.7.1, 15.5.22)
    for(auto const &[requiringStatus, field] : {std::pair{HttpStatus::unauthorized, "WWW-Authenticate"}, std::pair{HttpStatus::proxy_authentication_required, "Proxy-Authenticate"}, std::pair{HttpStatus::upgrade_required, "Upgrade"}}) {
        if(response.status == requiringStatus && findField(response.headers, field) == response.headers.end()) {
            ICHOR_LOG_ERROR(_logger, "HttpHostService {} handler responded with {} without the required {} header", getServiceId(), status, field);
        }
    }
    // 205 has no content (RFC 9110 15.3.6), but is framed like a response that has
    if(status == 205) {
        response.body.clear();
    }

    bool const switching = status == 101;
    // A 2xx to CONNECT starts a tunnel, which has no framing (RFC 9110 8.6, 9.3.6)
    bool const tunnel = context.method == HttpMethod::connect && status >= 200 && status < 300;
    if(!switching) {
        if(auto connection = findField(response.headers, "Connection"); connection != response.headers.end() && Detail::httpListContains(connection->second, "close")) {
            close = true;
        }
    }

    if(switching || tunnel || status == 204 || status == 304 || context.method == HttpMethod::head) {
        framing = ResponseFraming::NONE;
        // Only a streaming route can take a tunnel over
        if(tunnel && !streaming) {
            close = true;
        }
    } else if(!streaming || status == 205) {
        framing = ResponseFraming::CONTENT_LENGTH;
    } else if(context.minorVersion >= 1) {
        framing = ResponseFraming::CHUNKED;
    } else {
        // HTTP/1.0 doesn't have chunked (RFC 9112 6.1)
        framing = ResponseFraming::CLOSE_DELIMITED;
        close = true;
    }

    std::vector<uint8_t> msg;
    msg.reserve(512 + (framing == ResponseFraming::CONTENT_LENGTH ? response.body.size() : 0));
    // A client that asked to switch protocols and waits for a 100 Continue gets one first (RFC 9110 7.8)
    if(switching && context.owesContinue) {
        msg.insert(msg.end(), CONTINUE_RESPONSE.begin(), CONTINUE_RESPONSE.end());
    }
    auto statusText = ICHOR_STATUS_MATCHING.find(response.status);
    // The reason phrase can be empty (RFC 9112 4)
    fmt::format_to(FmtU8Inserter(msg), "HTTP/1.1 {} {}\r\n", status, statusText == ICHOR_STATUS_MATCHING.end() ? ""sv : statusText->second);
    // An origin server with a clock has to send Date (RFC 9110 6.6.1)
    if(findField(response.headers, "Date") == response.headers.end()) {
        fmt::format_to(FmtU8Inserter(msg), "Date: {}\r\n", Detail::formatHttpDate(std::chrono::system_clock::now()));
    }
    if(_sendServerHeader && findField(response.headers, "Server") == response.headers.end()) {
        fmt::format_to(FmtU8Inserter(msg), "Server: Ichor\r\n");
    }

    tl::optional<std::string_view> handlerContentLength;
    for(auto const &[k, v] : response.headers) {
        // The framing and the connection are this service's. A HEAD or 304 response can say what the length of the content would have been (RFC 9110 8.6).
        if(HttpHeaderNameEqual{}(k, "Content-Length")) {
            if(!v.empty() && IsOnlyDigits(v)) {
                handlerContentLength = v;
            }
            continue;
        }
        if(HttpHeaderNameEqual{}(k, "Transfer-Encoding") || HttpHeaderNameEqual{}(k, "Connection")) {
            continue;
        }
        fmt::format_to(FmtU8Inserter(msg), "{}: {}\r\n", k, v);
    }

    if(response.contentType && status >= 200 && status != 204 && status != 304 && !tunnel) {
        fmt::format_to(FmtU8Inserter(msg), "Content-Type: {}\r\n", *response.contentType);
    }

    // 1xx, 204 and tunnels have no Content-Length, 304 and HEAD can have the length of the GET response (RFC 9110 8.6, 9.3.2, 15.4.5)
    switch(framing) {
        case ResponseFraming::CONTENT_LENGTH:
            fmt::format_to(FmtU8Inserter(msg), "Content-Length: {}\r\n", response.body.size());
            break;
        case ResponseFraming::CHUNKED:
            fmt::format_to(FmtU8Inserter(msg), "Transfer-Encoding: chunked\r\n");
            break;
        case ResponseFraming::CLOSE_DELIMITED:
            break;
        case ResponseFraming::NONE:
            if(status == 304 || (context.method == HttpMethod::head && status >= 200 && status != 204 && !tunnel)) {
                if(handlerContentLength && response.body.empty()) {
                    fmt::format_to(FmtU8Inserter(msg), "Content-Length: {}\r\n", *handlerContentLength);
                } else if(status != 304 && !streaming) {
                    fmt::format_to(FmtU8Inserter(msg), "Content-Length: {}\r\n", response.body.size());
                }
            }
            break;
    }

    // Persistence (RFC 9112 9.3), an HTTP/1.0 client has to be told the connection stays open. Upgrade is a connection option, also when it
    // only lists protocols to switch to, e.g. in a 426 (RFC 9110 7.8).
    if(switching) {
        fmt::format_to(FmtU8Inserter(msg), "Connection: Upgrade\r\n");
    } else {
        bool const upgrade = findField(response.headers, "Upgrade") != response.headers.end();
        std::string_view const persistence = close ? "close"sv : context.minorVersion == 0 ? "keep-alive"sv : ""sv;
        if(upgrade && !persistence.empty()) {
            fmt::format_to(FmtU8Inserter(msg), "Connection: Upgrade, {}\r\n", persistence);
        } else if(upgrade) {
            fmt::format_to(FmtU8Inserter(msg), "Connection: Upgrade\r\n");
        } else if(!persistence.empty()) {
            fmt::format_to(FmtU8Inserter(msg), "Connection: {}\r\n", persistence);
        }
    }
    fmt::format_to(FmtU8Inserter(msg), "\r\n");

    if(framing == ResponseFraming::CONTENT_LENGTH) {
        msg.insert(msg.end(), response.body.begin(), response.body.end());
    }

    if(_debug) {
        ICHOR_LOG_INFO(_logger, "HttpHostService {} response {}", getServiceId(), std::string_view{reinterpret_cast<char const*>(msg.data()), msg.size()});
    }

    return SerializedResponse{.msg = std::move(msg), .framing = framing, .close = close, .replaced = replaced};
}

Ichor::Task<bool> Ichor::v1::HttpHostService::sendResponse(ServiceIdType id, HttpResponse &response, ResponseContext const &context, bool keepAlive) {
    auto serialized = serializeResponse(response, context, false, !keepAlive);
    if(!co_await send(id, std::move(serialized.msg))) {
        co_return true;
    }
    co_return serialized.close;
}

Ichor::Task<bool> Ichor::v1::HttpHostService::send(ServiceIdType id, std::vector<uint8_t> &&msg) {
    auto client = _connections.find(id);
    if(client == _connections.end()) {
        co_return false;
    }

    auto ret = co_await client->second->sendAsync(std::move(msg));
    if(!ret) {
        ICHOR_LOG_TRACE(_logger, "HttpHostService {} connection {} send failed {}", getServiceId(), id, ret.error());
        stopConnection(id);
        co_return false;
    }
    co_return true;
}

void Ichor::v1::HttpHostService::closeConnection(ServiceIdType id) {
    auto stateIt = _connectionStates.find(id);
    if(stateIt == _connectionStates.end() || stateIt->second.stopRequested || stateIt->second.phase == ConnectionPhase::LINGERING) {
        return;
    }
    auto &state = stateIt->second;
    state.closing = true;
    state.clearBuffer();
    if(state.upgraded) {
        state.upgraded->detach();
        state.upgraded.reset();
    }

    auto connection = _connections.find(id);
    if(!_timeoutTimer || _lingerTimeout.count() == 0 || connection == _connections.end()) {
        stopConnection(id);
        return;
    }
    // Closing with unread data makes the client lose the last response to a reset: close the sending side, and wait for the client to close
    // its side, which stops the connection service, or the linger timeout (RFC 9112 9.6)
    connection->second->shutdownSend();
    state.phase = ConnectionPhase::LINGERING;
    state.deadline = std::chrono::steady_clock::now() + _lingerTimeout;
}

void Ichor::v1::HttpHostService::stopConnection(ServiceIdType id) {
    auto state = _connectionStates.find(id);
    if(state == _connectionStates.end() || state->second.stopRequested) {
        return;
    }
    state->second.closing = true;
    state->second.stopRequested = true;
    state->second.clearBuffer();
    // Stopping the connection service closes the connection
    _queue->pushPrioritisedEvent<StopServiceEvent>(getServiceId(), _priority, id, true);
}

void Ichor::v1::HttpHostService::setIdleOrRequestPhase(ConnectionState &state) {
    if(state.pending().empty()) {
        state.phase = ConnectionPhase::IDLE;
        state.deadline = std::chrono::steady_clock::now() + _idleTimeout;
    } else {
        state.phase = ConnectionPhase::REQUEST;
        state.deadline = std::chrono::steady_clock::now() + _requestTimeout;
    }
}

void Ichor::v1::HttpHostService::startTimeoutTimer() {
    if(_timeoutTimer || !_timerFactory) {
        return;
    }
    // Checked often enough for the timeouts to be accurate to a quarter of the shortest one
    std::chrono::milliseconds shortest{std::chrono::milliseconds::max()};
    for(auto const timeout : {_idleTimeout, _requestTimeout, _lingerTimeout}) {
        if(timeout.count() > 0) {
            shortest = std::min(shortest, timeout);
        }
    }
    if(shortest == std::chrono::milliseconds::max()) {
        return;
    }
    _timeoutTimer = _timerFactory->createTimer();
    _timeoutTimer->setChronoInterval(std::clamp(shortest / 4, std::chrono::milliseconds{10}, std::chrono::milliseconds{1'000}));
    _timeoutTimer->setCallback([this]() {
        checkTimeouts();
    });
    _timeoutTimer->startTimer();
}

void Ichor::v1::HttpHostService::stopTimeoutTimer() {
    // Not stopped through the factory: it stops all timers of this service when this service stops, and can't be asked for a timer while it
    // does, which can be before this. checkTimeouts() does nothing once stopped.
    _timeoutTimer.reset();
}

void Ichor::v1::HttpHostService::checkTimeouts() {
    if(!_started) {
        return;
    }
    auto const now = std::chrono::steady_clock::now();
    std::vector<ServiceIdType> idle;
    std::vector<ServiceIdType> requests;
    std::vector<ServiceIdType> lingering;
    for(auto const &[id, state] : _connectionStates) {
        if(state.stopRequested || now < state.deadline) {
            continue;
        }
        if(state.phase == ConnectionPhase::IDLE && _idleTimeout.count() > 0) {
            idle.push_back(id);
        } else if(state.phase == ConnectionPhase::REQUEST && _requestTimeout.count() > 0 && !state.processing && !state.closing) {
            requests.push_back(id);
        } else if(state.phase == ConnectionPhase::LINGERING) {
            lingering.push_back(id);
        }
    }

    for(auto id : idle) {
        ICHOR_LOG_TRACE(_logger, "HttpHostService {} connection {} idle for too long, closing", getServiceId(), id);
        closeConnection(id);
    }
    for(auto id : lingering) {
        stopConnection(id);
    }
    for(auto id : requests) {
        ICHOR_LOG_TRACE(_logger, "HttpHostService {} connection {} request took too long, responding with 408", getServiceId(), id);
        auto &state = _connectionStates.find(id)->second;
        state.closing = true;
        state.clearBuffer();
        // The client can't know when the server gave up on its request, the connection is closed after this (RFC 9110 15.5.9)
        _queue->pushPrioritisedEvent<RunFunctionEventAsync>(getServiceId(), _priority, [this, id]() -> AsyncGenerator<IchorBehaviour> {
            HttpResponse response{.status = HttpStatus::request_timeout, .contentType = {}, .body = {}, .headers = {}};
            co_await sendResponse(id, response, ResponseContext{}, false);
            closeConnection(id);
            co_return {};
        });
    }
}

Ichor::v1::HttpRouteRegistration Ichor::v1::HttpHostService::addRoute(HttpMethod method, std::string_view route, std::function<Task<HttpResponse>(HttpRequest&)> handler) {
    return addRoute(method, std::make_unique<StringRouteMatcher>(route), std::move(handler));
}

Ichor::v1::HttpRouteRegistration Ichor::v1::HttpHostService::addRoute(HttpMethod method, std::unique_ptr<RouteMatcher> newMatcher, std::function<Task<HttpResponse>(HttpRequest&)> handler) {
    newMatcher->set_id(_matchersIdCounter);
    _handlers[method].emplace(std::move(newMatcher), RouteHandler{.handler = std::move(handler), .streamingHandler = {}});
    return {method, _matchersIdCounter++, this};
}

Ichor::v1::HttpRouteRegistration Ichor::v1::HttpHostService::addStreamingRoute(HttpMethod method, std::unique_ptr<RouteMatcher> newMatcher, HttpStreamingHandler handler) {
    newMatcher->set_id(_matchersIdCounter);
    _handlers[method].emplace(std::move(newMatcher), RouteHandler{.handler = {}, .streamingHandler = std::move(handler)});
    return {method, _matchersIdCounter++, this};
}

void Ichor::v1::HttpHostService::removeRoute(HttpMethod method, RouteIdType id) {
    auto routes = _handlers.find(method);

    if(routes == std::end(_handlers)) {
        return;
    }

    std::erase_if(routes->second, [id](auto const &item) {
       return item.first->get_id() == id;
    });
}

void Ichor::v1::HttpHostService::setPriority(uint64_t priority) {
    _priority = priority;
}

uint64_t Ichor::v1::HttpHostService::getPriority() {
    return _priority;
}
