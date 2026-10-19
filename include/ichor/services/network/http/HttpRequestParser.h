#pragma once

#include <ichor/services/network/http/HttpCommon.h>
#include <tl/optional.h>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The HTTP/1.1 request parsing and date handling of HttpHostService, see RFC 9110 (semantics) and RFC 9112 (message syntax).
namespace Ichor::v1::Detail {
    struct HttpRequestLimits {
        /// Longer request lines are answered with 414 URI Too Long
        uint64_t maxRequestLineSize{8ULL * 1024};
        /// Longer header sections (request line included) are answered with 431 Request Header Fields Too Large
        uint64_t maxHeaderSectionSize{64ULL * 1024};
        /// Larger bodies are answered with 413 Content Too Large
        uint64_t maxBodySize{512ULL * 1024 * 1024};
    };

    struct ParsedHttpRequest {
        /// HttpMethod::unknown for a method Ichor doesn't know, which has to be answered with 501 Not Implemented
        HttpMethod method{HttpMethod::unknown};
        /// origin-form (absolute-form requests are converted to it), authority-form for CONNECT or "*" for OPTIONS
        std::string target;
        /// HTTP/1.minorVersion
        uint8_t minorVersion{1};
        HttpHeaders headers;
        /// Without the transfer coding
        std::vector<uint8_t> body;
        /// Whether the connection can be used for another request after the response to this one
        bool keepAlive{true};
        /// An HTTP/1.1 request with Expect: 100-continue, whose client waits for a 100 Continue before sending the content
        bool expectContinue{};
        /// An absolute-form request target with the https scheme
        bool httpsTarget{};
        /// The request line and field lines as received, only for TRACE, which reflects them
        std::string head;
    };

    enum class HttpRequestParseStatus : uint_fast8_t {
        /// More data is needed
        INCOMPLETE,
        /// A complete request, which is the first consumed bytes of the buffer
        COMPLETE,
        /// The request has to be answered with errorStatus, after which the connection has to be closed: what follows can't be framed
        INVALID,
    };

    struct HttpRequestParseResult {
        HttpRequestParseStatus status{HttpRequestParseStatus::INCOMPLETE};
        /// COMPLETE: the length of the request in the buffer
        uint64_t consumed{};
        /// INVALID: the status to respond with
        HttpStatus errorStatus{HttpStatus::bad_request};
        /// INCOMPLETE: the header section has been received and the client waits for a 100 Continue before sending the body. COMPLETE: see
        /// ParsedHttpRequest::expectContinue.
        bool expectContinue{};
        /// COMPLETE
        ParsedHttpRequest request;
    };

    /// Where parsing a request that isn't complete yet left off, so that what was received of it isn't parsed again for every part that's
    /// received after it. Positions are relative to the start of the request line.
    struct HttpRequestParseState {
        /// The length of the empty lines before the request line
        uint64_t start{};
        /// Up to where line ends have been looked for, while the header section isn't complete
        uint64_t scanned{};
        /// Where the line that's being received starts
        uint64_t lineStart{};
        bool requestLineChecked{};
        /// The authority of an absolute-form request target, which replaces Host
        tl::optional<std::string> absoluteFormAuthority;
        /// The header section was parsed into request
        bool headerDone{};
        ParsedHttpRequest request;
        uint64_t contentStart{};
        uint64_t contentLength{};
        bool chunked{};
        /// Chunked: where the next chunk starts, relative to contentStart
        uint64_t chunkPos{};
        /// Chunked: the chunk-size lines, extensions and CRLFs so far
        uint64_t framingSize{};
        /// Chunked: the last chunk was received, the trailer section starts at trailerStart
        bool lastChunk{};
        uint64_t trailerStart{};
        uint64_t trailerPos{};
    };

    /// Parses the first request in buffer. Leading empty lines are skipped (RFC 9112 2.2).
    [[nodiscard]] HttpRequestParseResult parseHttpRequest(std::string_view buffer, HttpRequestLimits const &limits);
    /// Parses the first request in buffer, continuing where an INCOMPLETE result for the same buffer, with only more appended to it since, left
    /// off. state has to be reset for another buffer, e.g. after a COMPLETE or INVALID result.
    [[nodiscard]] HttpRequestParseResult parseHttpRequest(std::string_view buffer, HttpRequestLimits const &limits, HttpRequestParseState &state);

    /// Formats t as an IMF-fixdate, e.g. "Sun, 06 Nov 1994 08:49:37 GMT" (RFC 9110 5.6.7)
    [[nodiscard]] std::string formatHttpDate(std::chrono::system_clock::time_point t);
    /// Parses an HTTP-date in any of the three formats a recipient has to accept: IMF-fixdate, RFC 850 and asctime (RFC 9110 5.6.7)
    [[nodiscard]] tl::optional<std::chrono::sys_seconds> parseHttpDate(std::string_view date);

    /// tchar (RFC 9110 5.6.2)
    [[nodiscard]] bool isHttpTokenChar(char c) noexcept;
    /// token: 1*tchar
    [[nodiscard]] bool isHttpToken(std::string_view str) noexcept;
    /// field-value: VCHAR, obs-text, SP and HTAB only (RFC 9110 5.5)
    [[nodiscard]] bool isHttpFieldValue(std::string_view str) noexcept;
    /// Whether the comma separated list in value contains token, ignoring case (e.g. "close" in a Connection field)
    [[nodiscard]] bool httpListContains(std::string_view value, std::string_view token) noexcept;
}
