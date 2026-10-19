#include <ichor/services/network/http/HttpRequestParser.h>
#include <ichor/services/network/http/HttpInternal.h>
#include <ichor/stl/StringUtils.h>
#include <fmt/format.h>
#include <array>
#include <limits>

namespace {
    using namespace Ichor::v1;
    using namespace Ichor::v1::Detail;

    // A chunk-size line longer than this is not a chunk-size line
    constexpr uint64_t MAX_CHUNK_LINE_SIZE = 4096;

    constexpr std::array<std::string_view, 7> DAY_NAMES{"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    constexpr std::array<std::string_view, 7> LONG_DAY_NAMES{"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    constexpr std::array<std::string_view, 12> MONTH_NAMES{"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    constexpr bool isDigit(char c) noexcept {
        return c >= '0' && c <= '9';
    }

    constexpr bool isAlpha(char c) noexcept {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    constexpr bool isHexDigit(char c) noexcept {
        return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    constexpr char toLower(char c) noexcept {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    }

    bool equalsIgnoreCase(std::string_view lhs, std::string_view rhs) noexcept {
        return HttpHeaderNameEqual{}(lhs, rhs);
    }

    /// Removes OWS (SP and HTAB) from both ends
    std::string_view trimOws(std::string_view str) noexcept {
        while(!str.empty() && (str.front() == ' ' || str.front() == '\t')) {
            str.remove_prefix(1);
        }
        while(!str.empty() && (str.back() == ' ' || str.back() == '\t')) {
            str.remove_suffix(1);
        }
        return str;
    }

    /// unreserved / sub-delims (RFC 3986 2.2, 2.3)
    constexpr bool isUnreservedOrSubDelim(char c) noexcept {
        switch(c) {
            case '-': case '.': case '_': case '~':
            case '!': case '$': case '&': case '\'': case '(': case ')': case '*': case '+': case ',': case ';': case '=':
                return true;
            default:
                return isAlpha(c) || isDigit(c);
        }
    }

    /// Whether str only consists of the allowed characters and valid pct-encoded octets
    template <typename Allowed>
    bool isValidUriPart(std::string_view str, Allowed &&allowed) noexcept {
        for(size_t i = 0; i < str.size(); i++) {
            if(str[i] == '%') {
                if(i + 2 >= str.size() || !isHexDigit(str[i + 1]) || !isHexDigit(str[i + 2])) {
                    return false;
                }
                i += 2;
            } else if(!allowed(str[i])) {
                return false;
            }
        }
        return true;
    }

    /// origin-form = absolute-path [ "?" query ] (RFC 9112 3.2.1)
    bool isValidOriginForm(std::string_view target) noexcept {
        if(target.empty() || target.front() != '/') {
            return false;
        }
        // pchar / "/" / "?", '?' starts the query, which also allows '/' and '?'
        return isValidUriPart(target, [](char c) {
            return isUnreservedOrSubDelim(c) || c == ':' || c == '@' || c == '/' || c == '?';
        });
    }

    /// IPv4address = dec-octet "." dec-octet "." dec-octet "." dec-octet (RFC 3986 3.2.2)
    bool isValidIpv4(std::string_view str) noexcept {
        size_t i{};
        for(int octet = 0; octet < 4; octet++) {
            if(octet > 0) {
                if(i >= str.size() || str[i] != '.') {
                    return false;
                }
                i++;
            }
            auto const start = i;
            while(i < str.size() && isDigit(str[i]) && i - start < 3) {
                i++;
            }
            auto const digits = str.substr(start, i - start);
            // No leading zeros
            if(digits.empty() || (digits.size() > 1 && digits.front() == '0') || *parseUint64(digits) > 255) {
                return false;
            }
        }
        return i == str.size();
    }

    /// Counts the h16 groups of part of an IPv6address, an ls32 at the end of it counts as two. False if part isn't valid.
    bool countIpv6Groups(std::string_view part, bool allowIpv4, size_t &groups) noexcept {
        if(part.empty()) {
            return true;
        }
        size_t i{};
        while(true) {
            auto const colon = part.find(':', i);
            auto const group = part.substr(i, colon == std::string_view::npos ? std::string_view::npos : colon - i);
            if(colon == std::string_view::npos && allowIpv4 && group.find('.') != std::string_view::npos) {
                groups += 2;
                return isValidIpv4(group);
            }
            if(group.empty() || group.size() > 4 || !std::ranges::all_of(group, isHexDigit)) {
                return false;
            }
            groups++;
            if(colon == std::string_view::npos) {
                return true;
            }
            i = colon + 1;
        }
    }

    /// IPv6address (RFC 3986 3.2.2): eight groups, or fewer with one "::" for the groups of zeros left out
    bool isValidIpv6(std::string_view str) noexcept {
        auto const doubleColon = str.find("::");
        size_t groups{};
        if(doubleColon == std::string_view::npos) {
            return countIpv6Groups(str, true, groups) && groups == 8;
        }
        if(str.find("::", doubleColon + 1) != std::string_view::npos) {
            return false;
        }
        return countIpv6Groups(str.substr(0, doubleColon), false, groups) && countIpv6Groups(str.substr(doubleColon + 2), true, groups) && groups <= 7;
    }

    /// What's between the brackets of an IP-literal: IPv6address / IPvFuture (RFC 3986 3.2.2)
    bool isValidIpLiteral(std::string_view str) noexcept {
        if(!str.empty() && (str.front() == 'v' || str.front() == 'V')) {
            // IPvFuture = "v" 1*HEXDIG "." 1*( unreserved / sub-delims / ":" )
            auto const dot = str.find('.');
            if(dot == std::string_view::npos || dot == 1 || dot + 1 == str.size()) {
                return false;
            }
            return std::ranges::all_of(str.substr(1, dot - 1), isHexDigit) &&
                   std::ranges::all_of(str.substr(dot + 1), [](char c) { return isUnreservedOrSubDelim(c) || c == ':'; });
        }
        return isValidIpv6(str);
    }

    constexpr uint64_t saturatingAdd(uint64_t a, uint64_t b) noexcept {
        return a > std::numeric_limits<uint64_t>::max() - b ? std::numeric_limits<uint64_t>::max() : a + b;
    }

    struct HostAndPort {
        std::string_view host;
        tl::optional<std::string_view> port;
    };

    /// uri-host [ ":" port ] (RFC 9110 7.2)
    tl::optional<HostAndPort> parseHostAndPort(std::string_view hostport) noexcept {
        HostAndPort ret;
        std::string_view rest;
        if(!hostport.empty() && hostport.front() == '[') {
            // IP-literal, IPv6address or IPvFuture
            auto const close = hostport.find(']');
            if(close == std::string_view::npos || close == 1) {
                return tl::nullopt;
            }
            ret.host = hostport.substr(0, close + 1);
            if(!isValidIpLiteral(ret.host.substr(1, ret.host.size() - 2))) {
                return tl::nullopt;
            }
            rest = hostport.substr(close + 1);
        } else {
            // IPv4address or reg-name
            auto const colon = hostport.find(':');
            ret.host = hostport.substr(0, colon);
            if(!isValidUriPart(ret.host, isUnreservedOrSubDelim)) {
                return tl::nullopt;
            }
            rest = colon == std::string_view::npos ? std::string_view{} : hostport.substr(colon);
        }
        if(!rest.empty()) {
            if(rest.front() != ':' || !IsOnlyDigits(rest.substr(1))) {
                return tl::nullopt;
            }
            ret.port = rest.substr(1);
        }
        return ret;
    }

    /// The value of a Host field: empty, or uri-host [ ":" port ]
    bool isValidHostField(std::string_view value) noexcept {
        if(value.empty()) {
            return true;
        }
        auto const hostAndPort = parseHostAndPort(value);
        return hostAndPort && !hostAndPort->host.empty();
    }

    /// authority-form = uri-host ":" port (RFC 9112 3.2.3)
    bool isValidAuthorityForm(std::string_view target) noexcept {
        auto const hostAndPort = parseHostAndPort(target);
        if(!hostAndPort || hostAndPort->host.empty() || !hostAndPort->port || hostAndPort->port->empty()) {
            return false;
        }
        // CONNECT to an empty or invalid port is rejected (RFC 9110 9.3.6)
        auto const port = parseUint64(*hostAndPort->port);
        return port && *port >= 1 && *port <= 65'535;
    }

    /// chunk-size = 1*HEXDIG, nothing if it's empty or doesn't fit
    tl::optional<uint64_t> parseChunkSize(std::string_view hex) noexcept {
        if(hex.empty()) {
            return tl::nullopt;
        }
        uint64_t size{};
        for(char const c : hex) {
            if(size > (std::numeric_limits<uint64_t>::max() >> 4)) {
                return tl::nullopt;
            }
            auto const digit = isDigit(c) ? c - '0' : toLower(c) - 'a' + 10;
            size = (size << 4) | static_cast<uint64_t>(digit);
        }
        return size;
    }

    /// quoted-string (RFC 9110 5.6.4), starting at str[i]. Returns the index after the closing quote.
    tl::optional<size_t> skipQuotedString(std::string_view str, size_t i) noexcept {
        if(i >= str.size() || str[i] != '"') {
            return tl::nullopt;
        }
        for(i++; i < str.size(); i++) {
            auto const c = static_cast<unsigned char>(str[i]);
            if(c == '"') {
                return i + 1;
            }
            if(c == '\\') {
                i++;
                if(i >= str.size()) {
                    return tl::nullopt;
                }
                auto const escaped = static_cast<unsigned char>(str[i]);
                if(escaped != '\t' && (escaped < 0x20 || escaped == 0x7F)) {
                    return tl::nullopt;
                }
            } else if(c != '\t' && (c < 0x20 || c == 0x7F)) {
                return tl::nullopt;
            }
        }
        return tl::nullopt;
    }

    /// chunk-ext = *( BWS ";" BWS chunk-ext-name [ BWS "=" BWS chunk-ext-val ] ) (RFC 9112 7.1.1)
    bool isValidChunkExtensions(std::string_view ext) noexcept {
        size_t i{};
        auto skipBws = [&]() {
            while(i < ext.size() && (ext[i] == ' ' || ext[i] == '\t')) {
                i++;
            }
        };
        auto skipToken = [&]() {
            auto const start = i;
            while(i < ext.size() && isHttpTokenChar(ext[i])) {
                i++;
            }
            return i != start;
        };
        while(i < ext.size()) {
            skipBws();
            if(i >= ext.size() || ext[i] != ';') {
                return false;
            }
            i++;
            skipBws();
            if(!skipToken()) {
                return false;
            }
            skipBws();
            if(i < ext.size() && ext[i] == '=') {
                i++;
                skipBws();
                if(i < ext.size() && ext[i] == '"') {
                    auto const end = skipQuotedString(ext, i);
                    if(!end) {
                        return false;
                    }
                    i = *end;
                } else if(!skipToken()) {
                    return false;
                }
            }
        }
        return true;
    }

    struct FieldLine {
        std::string_view name;
        std::string_view value;
    };

    /// field-line = field-name ":" OWS field-value OWS (RFC 9112 5)
    tl::optional<FieldLine> parseFieldLine(std::string_view line) noexcept {
        auto const colon = line.find(':');
        if(colon == std::string_view::npos) {
            return tl::nullopt;
        }
        FieldLine ret{.name = line.substr(0, colon), .value = trimOws(line.substr(colon + 1))};
        // Also rejects whitespace between the name and the colon (RFC 9112 5.1)
        if(!isHttpToken(ret.name) || !isHttpFieldValue(ret.value)) {
            return tl::nullopt;
        }
        return ret;
    }

    /// Finds the end of the line starting at pos, which has to end in CRLF. Returns the index of the LF, npos if there's no LF yet, or nothing if the line is invalid.
    tl::optional<size_t> findLineEnd(std::string_view buffer, size_t pos) noexcept {
        auto const lf = buffer.find('\n', pos);
        if(lf == std::string_view::npos) {
            return lf;
        }
        // A bare LF, or a bare CR in the line (RFC 9112 2.2)
        if(lf == pos || buffer[lf - 1] != '\r' || buffer.substr(pos, lf - 1 - pos).find('\r') != std::string_view::npos) {
            return tl::nullopt;
        }
        return lf;
    }

    HttpRequestParseResult invalid(HttpStatus status) {
        HttpRequestParseResult ret;
        ret.status = HttpRequestParseStatus::INVALID;
        ret.errorStatus = status;
        return ret;
    }

    /// A request line longer than the limit: 501 if its method is longer than any Ichor implements, 414 otherwise (RFC 9112 3)
    HttpRequestParseResult requestLineTooLong(std::string_view requestLine) {
        size_t longestMethod{};
        for(auto const &[name, method] : ICHOR_METHOD_MATCHING) {
            longestMethod = std::max(longestMethod, name.size());
        }
        auto const methodEnd = requestLine.find(' ');
        if(methodEnd == std::string_view::npos ? requestLine.size() > longestMethod : methodEnd > longestMethod) {
            return invalid(HttpStatus::not_implemented);
        }
        return invalid(HttpStatus::uri_too_long);
    }

    HttpRequestParseResult incomplete(bool expectContinue = false) {
        HttpRequestParseResult ret;
        ret.status = HttpRequestParseStatus::INCOMPLETE;
        ret.expectContinue = expectContinue;
        return ret;
    }

    tl::optional<std::string_view> findName(std::string_view name, auto const &names) noexcept {
        for(auto const &n : names) {
            if(n == name) {
                return n;
            }
        }
        return tl::nullopt;
    }

    tl::optional<unsigned> parseMonth(std::string_view month) noexcept {
        for(unsigned i = 0; i < MONTH_NAMES.size(); i++) {
            if(MONTH_NAMES[i] == month) {
                return i + 1;
            }
        }
        return tl::nullopt;
    }

    tl::optional<unsigned> parseDigits(std::string_view str) noexcept {
        if(str.empty() || !IsOnlyDigits(str)) {
            return tl::nullopt;
        }
        auto const value = parseUint64(str);
        if(!value || *value > 9999) {
            return tl::nullopt;
        }
        return static_cast<unsigned>(*value);
    }

    /// time-of-day = hour ":" minute ":" second
    tl::optional<std::chrono::seconds> parseTimeOfDay(std::string_view time) noexcept {
        if(time.size() != 8 || time[2] != ':' || time[5] != ':') {
            return tl::nullopt;
        }
        auto const hour = parseDigits(time.substr(0, 2));
        auto const minute = parseDigits(time.substr(3, 2));
        auto const second = parseDigits(time.substr(6, 2));
        // 60 is a leap second
        if(!hour || !minute || !second || *hour > 23 || *minute > 59 || *second > 60) {
            return tl::nullopt;
        }
        return std::chrono::hours{*hour} + std::chrono::minutes{*minute} + std::chrono::seconds{*second};
    }

    tl::optional<std::chrono::sys_seconds> makeDate(unsigned year, unsigned month, unsigned day, std::chrono::seconds timeOfDay) noexcept {
        std::chrono::year_month_day const ymd{std::chrono::year{static_cast<int>(year)}, std::chrono::month{month}, std::chrono::day{day}};
        if(!ymd.ok()) {
            return tl::nullopt;
        }
        return std::chrono::sys_seconds{std::chrono::sys_days{ymd}.time_since_epoch()} + timeOfDay;
    }

    /// request-line = method SP request-target SP HTTP-version (RFC 9112 3), into state.request
    tl::optional<HttpRequestParseResult> parseRequestLine(std::string_view requestLine, HttpRequestParseState &state) {
        auto &req = state.request;
        auto const firstSpace = requestLine.find(' ');
        auto const secondSpace = firstSpace == std::string_view::npos ? std::string_view::npos : requestLine.find(' ', firstSpace + 1);
        if(secondSpace == std::string_view::npos) {
            return invalid(HttpStatus::bad_request);
        }
        auto const method = requestLine.substr(0, firstSpace);
        auto const target = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
        auto const version = requestLine.substr(secondSpace + 1);
        if(!isHttpToken(method) || target.empty()) {
            return invalid(HttpStatus::bad_request);
        }
        // Known before the rest is checked: an error response to HEAD has no content
        if(auto const methodIt = ICHOR_METHOD_MATCHING.find(method); methodIt != ICHOR_METHOD_MATCHING.end()) {
            req.method = methodIt->second;
        }

        // HTTP-version = "HTTP" "/" DIGIT "." DIGIT (RFC 9112 2.3)
        if(version.size() != 8 || !version.starts_with("HTTP/") || !isDigit(version[5]) || version[6] != '.' || !isDigit(version[7])) {
            return invalid(HttpStatus::bad_request);
        }
        if(version[5] != '1') {
            return invalid(HttpStatus::http_version_not_supported);
        }
        req.minorVersion = static_cast<uint8_t>(version[7] - '0');

        // request-target forms (RFC 9112 3.2)
        if(target == "*") {
            if(req.method != HttpMethod::options) {
                return invalid(HttpStatus::bad_request);
            }
            req.target = target;
        } else if(req.method == HttpMethod::connect) {
            if(!isValidAuthorityForm(target)) {
                return invalid(HttpStatus::bad_request);
            }
            req.target = target;
        } else if(target.front() == '/') {
            if(!isValidOriginForm(target)) {
                return invalid(HttpStatus::bad_request);
            }
            req.target = target;
        } else {
            // absolute-form, of which only http and https URIs can identify a resource on this server
            auto const schemeEnd = target.find("://");
            if(schemeEnd == std::string_view::npos || !(equalsIgnoreCase(target.substr(0, schemeEnd), "http") || equalsIgnoreCase(target.substr(0, schemeEnd), "https"))) {
                return invalid(HttpStatus::bad_request);
            }
            auto const rest = target.substr(schemeEnd + 3);
            auto const pathStart = rest.find_first_of("/?#");
            auto const authority = rest.substr(0, pathStart);
            // userinfo is an error (RFC 9110 4.2.4)
            if(authority.empty() || authority.find('@') != std::string_view::npos || !isValidHostField(authority)) {
                return invalid(HttpStatus::bad_request);
            }
            auto const pathAndQuery = pathStart == std::string_view::npos ? std::string_view{} : rest.substr(pathStart);
            if(pathAndQuery.empty() && req.method == HttpMethod::options) {
                // OPTIONS for an empty path and no query is OPTIONS * (RFC 9112 3.2.4)
                req.target = "*";
            } else {
                if(pathAndQuery.empty() || pathAndQuery.front() != '/') {
                    req.target = "/";
                }
                req.target += pathAndQuery;
                if(!isValidOriginForm(req.target)) {
                    return invalid(HttpStatus::bad_request);
                }
            }
            // A request for an https resource has to come over a connection secured for it (RFC 9110 7.4), which HttpHostService's aren't.
            // Rejected before its content arrives or a 100 Continue asks for it.
            if(equalsIgnoreCase(target.substr(0, schemeEnd), "https")) {
                return invalid(HttpStatus::misdirected_request);
            }
            state.absoluteFormAuthority = std::string{authority};
        }
        return tl::nullopt;
    }

    /// The field lines of a complete header section, msg up to headerEnd, into state.request, and how long the content is
    tl::optional<HttpRequestParseResult> parseHeaderSection(std::string_view msg, uint64_t headerEnd, HttpRequestLimits const &limits, HttpRequestParseState &state) {
        auto &req = state.request;
        // Later 1.x versions are compatible with 1.1 (RFC 9110 2.5)
        bool const http11 = req.minorVersion >= 1;

        // field lines up to an empty line (RFC 9112 5), the lines were checked for their CRLFs already
        size_t pos = msg.find("\r\n") + 2;
        uint64_t hostFields{};
        while(pos + 2 < headerEnd) {
            auto const lineEnd = msg.find("\r\n", pos);
            auto const line = msg.substr(pos, lineEnd - pos);
            pos = lineEnd + 2;
            // obs-fold, or whitespace between the start line and the first field line (RFC 9112 2.2, 5.2)
            if(line.front() == ' ' || line.front() == '\t') {
                return invalid(HttpStatus::bad_request);
            }
            auto const field = parseFieldLine(line);
            if(!field) {
                return invalid(HttpStatus::bad_request);
            }
            if(equalsIgnoreCase(field->name, "Host")) {
                hostFields++;
            }
            // Field lines with the same name form one comma separated list (RFC 9110 5.3)
            addHttpField(req.headers, field->name, field->value);
        }

        if(req.method == HttpMethod::trace) {
            req.head = msg.substr(0, headerEnd);
        }

        // Exactly one Host in HTTP/1.1, at most one in HTTP/1.0 (RFC 9112 3.2)
        if(hostFields > 1 || (http11 && hostFields == 0)) {
            return invalid(HttpStatus::bad_request);
        }
        if(auto const hostIt = req.headers.find("Host"); hostIt != req.headers.end() && !isValidHostField(hostIt->second)) {
            return invalid(HttpStatus::bad_request);
        }
        // The authority of an absolute-form target replaces Host (RFC 9112 3.2.2)
        if(state.absoluteFormAuthority) {
            req.headers.insert_or_assign(std::string{"Host"}, std::move(*state.absoluteFormAuthority));
        }

        // Persistence (RFC 9112 9.3)
        if(auto const connectionIt = req.headers.find("Connection"); connectionIt != req.headers.end()) {
            bool const close = httpListContains(connectionIt->second, "close");
            req.keepAlive = !close && (http11 || httpListContains(connectionIt->second, "keep-alive"));
        } else {
            req.keepAlive = http11;
        }

        // Message body length (RFC 9112 6.3)
        auto const transferEncodingIt = req.headers.find("Transfer-Encoding");
        auto const contentLengthIt = req.headers.find("Content-Length");
        state.chunked = transferEncodingIt != req.headers.end();
        if(state.chunked) {
            // Both are a possible request smuggling attempt, Transfer-Encoding in HTTP/1.0 is faulty framing (RFC 9112 6.1, 6.3)
            if(contentLengthIt != req.headers.end() || !http11) {
                return invalid(HttpStatus::bad_request);
            }
            // transfer-coding = token *( OWS ";" OWS transfer-parameter ) (RFC 9112 7)
            struct Coding {
                std::string_view name;
                bool parameters{};
            };
            std::vector<Coding> codings;
            bool valid{true};
            split(transferEncodingIt->second, ",", false, [&](std::string_view coding) {
                coding = trimOws(coding);
                // Empty list elements are ignored (RFC 9110 5.6.1)
                if(coding.empty()) {
                    return;
                }
                auto const parametersStart = coding.find(';');
                auto const name = trimOws(coding.substr(0, parametersStart));
                if(!isHttpToken(name)) {
                    valid = false;
                }
                codings.emplace_back(Coding{.name = name, .parameters = parametersStart != std::string_view::npos});
            });
            // chunked is the last coding, it has no parameters: one that a proxy in front reads differently would frame the request differently
            if(!valid || codings.empty() || !equalsIgnoreCase(codings.back().name, "chunked") || codings.back().parameters) {
                return invalid(HttpStatus::bad_request);
            }
            for(size_t i = 0; i + 1 < codings.size(); i++) {
                // chunked can only be applied once, and other codings are not supported (RFC 9112 6.1)
                if(equalsIgnoreCase(codings[i].name, "chunked")) {
                    return invalid(HttpStatus::bad_request);
                }
                return invalid(HttpStatus::not_implemented);
            }
        } else if(contentLengthIt != req.headers.end()) {
            // A list, even of identical values, is rejected rather than reduced to one value (RFC 9110 8.6 allows both): a proxy in front could have
            // picked another one
            auto const value = parseUint64(contentLengthIt->second);
            if(!value) {
                return invalid(HttpStatus::bad_request);
            }
            state.contentLength = *value;
            if(state.contentLength > limits.maxBodySize) {
                return invalid(HttpStatus::payload_too_large);
            }
        }

        // Expect (RFC 9110 10.1.1)
        if(auto const expectIt = req.headers.find("Expect"); expectIt != req.headers.end()) {
            // Expect = #expectation, empty list elements are ignored (RFC 9110 5.6.1.2)
            bool unknown{};
            bool expectsContinue{};
            split(expectIt->second, ",", false, [&](std::string_view expectation) {
                expectation = trimOws(expectation);
                if(expectation.empty()) {
                    return;
                }
                if(equalsIgnoreCase(expectation, "100-continue")) {
                    expectsContinue = true;
                } else {
                    unknown = true;
                }
            });
            if(unknown) {
                return invalid(HttpStatus::expectation_failed);
            }
            // A 100-continue expectation in an HTTP/1.0 request has to be ignored
            req.expectContinue = expectsContinue && http11;
        }

        state.contentStart = headerEnd;
        state.headerDone = true;
        return tl::nullopt;
    }

    /// Looks for the end of the header section in what was received since the last call, and parses the header section once it's complete.
    /// Returns the result if it isn't complete or is invalid.
    tl::optional<HttpRequestParseResult> receiveHeaderSection(std::string_view buffer, HttpRequestLimits const &limits, HttpRequestParseState &state) {
        if(state.scanned == 0) {
            // Empty lines before the request line are ignored (RFC 9112 2.2)
            while(buffer.size() - state.start >= 2 && buffer[state.start] == '\r' && buffer[state.start + 1] == '\n') {
                state.start += 2;
            }
            // They count towards the request line, endless empty lines are not a request
            if(state.start > limits.maxRequestLineSize) {
                return invalid(HttpStatus::bad_request);
            }
        }

        std::string_view const msg = buffer.substr(state.start);
        size_t i = state.scanned;
        tl::optional<uint64_t> headerEnd;
        while(i < msg.size()) {
            auto const lineEnd = msg.find_first_of("\r\n", i);
            if(lineEnd == std::string_view::npos) {
                i = msg.size();
                break;
            }
            // Lines end with CRLF, a bare LF or bare CR is invalid (RFC 9112 2.2)
            if(msg[lineEnd] == '\n') {
                return invalid(HttpStatus::bad_request);
            }
            if(lineEnd + 1 == msg.size()) {
                i = lineEnd;
                break;
            }
            if(msg[lineEnd + 1] != '\n') {
                return invalid(HttpStatus::bad_request);
            }
            i = lineEnd + 2;

            bool const requestLine = !state.requestLineChecked;
            if(requestLine) {
                // A request line longer than the server wants: 414, or 501 for a method longer than any implemented (RFC 9112 3)
                if(lineEnd > limits.maxRequestLineSize) {
                    return requestLineTooLong(msg.substr(0, lineEnd));
                }
                if(auto error = parseRequestLine(msg.substr(0, lineEnd), state)) {
                    return error;
                }
                state.requestLineChecked = true;
            }
            if(i > limits.maxHeaderSectionSize) {
                return invalid(HttpStatus::request_header_fields_too_large);
            }
            if(!requestLine && lineEnd == state.lineStart) {
                headerEnd = i;
                break;
            }
            state.lineStart = i;
        }
        state.scanned = i;

        if(!headerEnd) {
            if(!state.requestLineChecked) {
                return msg.size() > limits.maxRequestLineSize ? requestLineTooLong(msg) : incomplete();
            }
            return msg.size() > limits.maxHeaderSectionSize ? invalid(HttpStatus::request_header_fields_too_large) : incomplete();
        }
        return parseHeaderSection(msg, *headerEnd, limits, state);
    }

    /// chunked-body = *chunk last-chunk trailer-section CRLF (RFC 9112 7.1), continuing from where the last call left off. Returns the result if it
    /// isn't complete or is invalid, otherwise the body ends at state.trailerPos.
    tl::optional<HttpRequestParseResult> receiveChunkedBody(std::string_view content, HttpRequestLimits const &limits, HttpRequestParseState &state) {
        auto &body = state.request.body;
        bool const expectContinue = state.request.expectContinue;
        while(!state.lastChunk) {
            auto const lineEnd = findLineEnd(content, state.chunkPos);
            if(!lineEnd) {
                return invalid(HttpStatus::bad_request);
            }
            if(*lineEnd == std::string_view::npos) {
                return content.size() - state.chunkPos > MAX_CHUNK_LINE_SIZE ? invalid(HttpStatus::bad_request) : incomplete(expectContinue);
            }
            auto const line = content.substr(state.chunkPos, *lineEnd - 1 - state.chunkPos);
            if(line.size() > MAX_CHUNK_LINE_SIZE) {
                return invalid(HttpStatus::bad_request);
            }
            auto const sizeEnd = static_cast<size_t>(std::ranges::find_if_not(line, isHexDigit) - line.begin());
            auto const chunkSize = parseChunkSize(line.substr(0, sizeEnd));
            if(!chunkSize || !isValidChunkExtensions(line.substr(sizeEnd))) {
                return invalid(HttpStatus::bad_request);
            }
            // The framing can't be more than the content and a header section's worth: chunk extensions and leading zeros are no content, but
            // would be received endlessly otherwise. A body of one byte chunks still fits.
            uint64_t const framingSize = state.framingSize + line.size() + (*chunkSize == 0 ? 2 : 4);
            if(framingSize > saturatingAdd(limits.maxHeaderSectionSize, saturatingAdd(body.size(), *chunkSize))) {
                return invalid(HttpStatus::bad_request);
            }
            auto const dataStart = *lineEnd + 1;

            if(*chunkSize == 0) {
                state.framingSize = framingSize;
                state.lastChunk = true;
                state.trailerStart = dataStart;
                state.trailerPos = dataStart;
                break;
            }

            if(*chunkSize > limits.maxBodySize - body.size()) {
                return invalid(HttpStatus::payload_too_large);
            }
            if(content.size() - dataStart < 2 || content.size() - dataStart - 2 < *chunkSize) {
                return incomplete(expectContinue);
            }
            auto const data = content.substr(dataStart, static_cast<size_t>(*chunkSize));
            if(content.substr(dataStart + data.size(), 2) != "\r\n") {
                return invalid(HttpStatus::bad_request);
            }
            body.insert(body.end(), data.begin(), data.end());
            state.framingSize = framingSize;
            state.chunkPos = dataStart + data.size() + 2;
        }

        // trailer-section, which is discarded (RFC 9112 7.1.2)
        while(true) {
            auto const trailerEnd = findLineEnd(content, state.trailerPos);
            if(!trailerEnd) {
                return invalid(HttpStatus::bad_request);
            }
            if(*trailerEnd == std::string_view::npos) {
                return content.size() - state.trailerStart > limits.maxHeaderSectionSize ? invalid(HttpStatus::request_header_fields_too_large) : incomplete(expectContinue);
            }
            if(*trailerEnd + 1 - state.trailerStart > limits.maxHeaderSectionSize) {
                return invalid(HttpStatus::request_header_fields_too_large);
            }
            auto const trailer = content.substr(state.trailerPos, *trailerEnd - 1 - state.trailerPos);
            state.trailerPos = *trailerEnd + 1;
            if(trailer.empty()) {
                return tl::nullopt;
            }
            if(trailer.front() == ' ' || trailer.front() == '\t' || !parseFieldLine(trailer)) {
                return invalid(HttpStatus::bad_request);
            }
        }
    }
}

namespace Ichor::v1::Detail {
    bool isHttpTokenChar(char c) noexcept {
        switch(c) {
            case '!': case '#': case '$': case '%': case '&': case '\'': case '*': case '+': case '-': case '.': case '^': case '_': case '`': case '|': case '~':
                return true;
            default:
                return isAlpha(c) || isDigit(c);
        }
    }

    bool isHttpToken(std::string_view str) noexcept {
        return !str.empty() && std::ranges::all_of(str, isHttpTokenChar);
    }

    bool isHttpFieldValue(std::string_view str) noexcept {
        return std::ranges::all_of(str, [](char c) {
            auto const uc = static_cast<unsigned char>(c);
            return uc == '\t' || (uc >= 0x20 && uc != 0x7F);
        });
    }

    bool httpListContains(std::string_view value, std::string_view token) noexcept {
        bool found{};
        split(value, ",", false, [&](std::string_view element) {
            if(equalsIgnoreCase(trimOws(element), token)) {
                found = true;
            }
        });
        return found;
    }

    std::string formatHttpDate(std::chrono::system_clock::time_point t) {
        auto const secs = std::chrono::floor<std::chrono::seconds>(t);
        auto const days = std::chrono::floor<std::chrono::days>(secs);
        std::chrono::year_month_day const ymd{days};
        std::chrono::weekday const weekday{days};
        std::chrono::hh_mm_ss const time{secs - days};
        return fmt::format("{}, {:02} {} {:04} {:02}:{:02}:{:02} GMT", DAY_NAMES[weekday.c_encoding()], static_cast<unsigned>(ymd.day()), MONTH_NAMES[static_cast<unsigned>(ymd.month()) - 1],
                           static_cast<int>(ymd.year()), time.hours().count(), time.minutes().count(), time.seconds().count());
    }

    tl::optional<std::chrono::sys_seconds> parseHttpDate(std::string_view date) {
        auto const comma = date.find(',');
        if(comma == std::string_view::npos) {
            // asctime-date = day-name SP month SP ( 2DIGIT / ( SP DIGIT ) ) SP time-of-day SP year, e.g. "Sun Nov  6 08:49:37 1994"
            if(date.size() != 24 || !findName(date.substr(0, 3), DAY_NAMES) || date[3] != ' ' || date[7] != ' ' || date[10] != ' ' || date[19] != ' ') {
                return tl::nullopt;
            }
            auto const month = parseMonth(date.substr(4, 3));
            auto const day = parseDigits(date[8] == ' ' ? date.substr(9, 1) : date.substr(8, 2));
            auto const time = parseTimeOfDay(date.substr(11, 8));
            auto const year = parseDigits(date.substr(20, 4));
            if(!month || !day || !time || !year) {
                return tl::nullopt;
            }
            return makeDate(*year, *month, *day, *time);
        }

        auto const dayName = date.substr(0, comma);
        if(findName(dayName, DAY_NAMES)) {
            // IMF-fixdate = day-name "," SP day SP month SP year SP time-of-day SP GMT, e.g. "Sun, 06 Nov 1994 08:49:37 GMT"
            if(date.size() != 29 || date[4] != ' ' || date[7] != ' ' || date[11] != ' ' || date[16] != ' ' || date[25] != ' ' || date.substr(26) != "GMT") {
                return tl::nullopt;
            }
            auto const day = parseDigits(date.substr(5, 2));
            auto const month = parseMonth(date.substr(8, 3));
            auto const year = parseDigits(date.substr(12, 4));
            auto const time = parseTimeOfDay(date.substr(17, 8));
            if(!month || !day || !time || !year) {
                return tl::nullopt;
            }
            return makeDate(*year, *month, *day, *time);
        }

        if(findName(dayName, LONG_DAY_NAMES)) {
            // rfc850-date = day-name-l "," SP 2DIGIT "-" month "-" 2DIGIT SP time-of-day SP GMT, e.g. "Sunday, 06-Nov-94 08:49:37 GMT"
            auto const rest = date.substr(comma + 1);
            if(rest.size() != 23 || rest[0] != ' ' || rest[3] != '-' || rest[7] != '-' || rest[10] != ' ' || rest[19] != ' ' || rest.substr(20) != "GMT") {
                return tl::nullopt;
            }
            auto const day = parseDigits(rest.substr(1, 2));
            auto const month = parseMonth(rest.substr(4, 3));
            auto const shortYear = parseDigits(rest.substr(8, 2));
            auto const time = parseTimeOfDay(rest.substr(11, 8));
            if(!month || !day || !time || !shortYear) {
                return tl::nullopt;
            }
            // A two-digit year more than 50 years in the future is the most recent year in the past with those digits (RFC 9110 5.6.7)
            auto const currentYear = static_cast<unsigned>(static_cast<int>(std::chrono::year_month_day{std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())}.year()));
            auto year = currentYear - currentYear % 100 + *shortYear;
            if(year > currentYear + 50) {
                year -= 100;
            }
            return makeDate(year, *month, *day, *time);
        }

        return tl::nullopt;
    }

    HttpRequestParseResult parseHttpRequest(std::string_view buffer, HttpRequestLimits const &limits) {
        HttpRequestParseState state;
        return parseHttpRequest(buffer, limits, state);
    }

    HttpRequestParseResult parseHttpRequest(std::string_view buffer, HttpRequestLimits const &limits, HttpRequestParseState &state) {
        if(!state.headerDone) {
            if(auto result = receiveHeaderSection(buffer, limits, state)) {
                return std::move(*result);
            }
        }

        // Message body (RFC 9112 6)
        auto const content = buffer.substr(state.start + state.contentStart);
        uint64_t contentEnd{};
        if(!state.chunked) {
            if(content.size() < state.contentLength) {
                return incomplete(state.request.expectContinue);
            }
            state.request.body.assign(content.begin(), content.begin() + static_cast<std::ptrdiff_t>(state.contentLength));
            contentEnd = state.contentLength;
        } else {
            if(auto result = receiveChunkedBody(content, limits, state)) {
                return std::move(*result);
            }
            contentEnd = state.trailerPos;
        }

        HttpRequestParseResult ret;
        ret.status = HttpRequestParseStatus::COMPLETE;
        ret.consumed = state.start + state.contentStart + contentEnd;
        ret.expectContinue = state.request.expectContinue;
        ret.request = std::move(state.request);
        return ret;
    }
}
