#pragma once

#include <ichor/services/network/ISSL.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>

namespace Ichor::v1 {
    class OpenSSLConnection final : public TLSConnection {
    public:
        OpenSSLConnection(SSL* ctx, TLSConnectionIdType id);
        ~OpenSSLConnection() final;

        SSL* getHandle() const;

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        [[nodiscard]] TLSConnectionTypeType getType() const noexcept final;
#endif

        ::BIO *internalBio{};
        ::BIO *externalBio{};
    };
}
