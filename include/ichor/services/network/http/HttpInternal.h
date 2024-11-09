#pragma once

#include <ichor/Common.h>
#include <ichor/services/network/http/HttpCommon.h>
#include <string_view>

namespace Ichor {
    extern unordered_map<std::string_view, HttpMethod> ICHOR_METHOD_MATCHING;
    extern unordered_map<HttpMethod, std::string_view> ICHOR_REVERSE_METHOD_MATCHING;
    extern unordered_map<HttpStatus, std::string_view> ICHOR_STATUS_MATCHING;
    constexpr std::string_view ICHOR_HTTP_VERSION_MATCH = "HTTP/1.1";

    enum class HttpParseError : int_fast16_t {
        NONE,
        BADREQUEST,
        WRONGHTTPVERSION,
        QUITTING,
        BUFFEROVERFLOW
    };
}

template <>
struct fmt::formatter<Ichor::HttpParseError> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::HttpParseError& change, FormatContext& ctx) const {
        switch(change) {
            case Ichor::HttpParseError::NONE:
                return fmt::format_to(ctx.out(), "NONE");
            case Ichor::HttpParseError::BADREQUEST:
                return fmt::format_to(ctx.out(), "BADREQUEST");
            case Ichor::HttpParseError::WRONGHTTPVERSION:
                return fmt::format_to(ctx.out(), "WRONGHTTPVERSION");
            case Ichor::HttpParseError::QUITTING:
                return fmt::format_to(ctx.out(), "QUITTING");
            case Ichor::HttpParseError::BUFFEROVERFLOW:
                return fmt::format_to(ctx.out(), "BUFFEROVERFLOW");
        }
        return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
    }
};
