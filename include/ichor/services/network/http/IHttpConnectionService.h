#pragma once

#include <ichor/coroutines/Task.h>
#include <tl/expected.h>
#include "HttpCommon.h"

namespace Ichor {
    enum class HttpError {
        NO_CONNECTION,
        WRONG_METHOD,
        UNABLE_TO_PARSE_HEADER,
        UNABLE_TO_PARSE_RESPONSE,
        IO_ERROR,
        GET_REQUESTS_CANNOT_HAVE_BODY,
        SVC_QUITTING,
        BOOST_READ_OR_WRITE_ERROR
    };

    class IHttpConnectionService {
    public:

        /**
         * Send message asynchronously to the connected http server. Use co_await to get the response.
         * @param method method type (GET, POST, etc)
         * @param route The route, or path, of this request. Has to be pointing to valid memory until an HttpResponseEvent is received.
         * @param msg Usually json, ignored for GET requests
         * @return response
         */
        virtual Task<tl::expected<HttpResponse, HttpError>> sendAsync(HttpMethod method, std::string_view route, unordered_map<std::string, std::string> &&headers, std::vector<uint8_t>&& msg) = 0;

        /**
         * Close the connection
         * @return true if closed, false if already closed
         */
        virtual Task<void> close() = 0;

        /**
         * Sets priority with which to push incoming network events.
         * @param priority
         */
        virtual void setPriority(uint64_t priority) = 0;
        virtual uint64_t getPriority() = 0;

    protected:
        ~IHttpConnectionService() = default;
    };
}

template <>
struct fmt::formatter<Ichor::HttpError> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::HttpError& state, FormatContext& ctx) const {
        switch(state)
        {
            case Ichor::HttpError::NO_CONNECTION:
                return fmt::format_to(ctx.out(), "NO_CONNECTION");
            case Ichor::HttpError::WRONG_METHOD:
                return fmt::format_to(ctx.out(), "WRONG_METHOD");
            case Ichor::HttpError::UNABLE_TO_PARSE_HEADER:
                return fmt::format_to(ctx.out(), "UNABLE_TO_PARSE_HEADER");
            case Ichor::HttpError::UNABLE_TO_PARSE_RESPONSE:
                return fmt::format_to(ctx.out(), "UNABLE_TO_PARSE_RESPONSE");
            case Ichor::HttpError::IO_ERROR:
                return fmt::format_to(ctx.out(), "IO_ERROR");
            case Ichor::HttpError::GET_REQUESTS_CANNOT_HAVE_BODY:
                return fmt::format_to(ctx.out(), "GET_REQUESTS_CANNOT_HAVE_BODY");
            case Ichor::HttpError::SVC_QUITTING:
                return fmt::format_to(ctx.out(), "SVC_QUITTING");
            case Ichor::HttpError::BOOST_READ_OR_WRITE_ERROR:
                return fmt::format_to(ctx.out(), "BOOST_READ_OR_WRITE_ERROR");
        }
        return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
    }
};
