#pragma once

#include <cstdint>
#include <string_view>
#include <tl/expected.h>

namespace Ichor::v1 {
    struct TLSContext {
        TLSContext(void *ctx) : _ctx{ctx} {}
        virtual ~TLSContext() = default;

        // virtual

    protected:
        void *_ctx{};
    };

    struct TLSConnection {
        virtual ~TLSConnection() = default;

    };

    enum class TLSContextError {
        UNKNOWN
    };

    enum class TLSConnectionError {

    };

    class ISSL {
    public:

        virtual tl::expected<TLSContext, TLSContextError> createTLSContext() = 0;
        virtual tl::expected<TLSConnection, TLSConnectionError> createTLSConnection(TLSContext&) = 0;
        virtual tl::expected<void, bool> TLSWrite(TLSContext&, std::string_view) = 0;
        virtual tl::expected<void, bool> TLSRead(TLSContext&, std::string_view) = 0;

    protected:
        ~ISSL() = default;
    };
}
