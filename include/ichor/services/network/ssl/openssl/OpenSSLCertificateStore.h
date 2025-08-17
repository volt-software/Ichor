#pragma once

#include <ichor/services/network/ISSL.h>
#include <openssl/x509_vfy.h>
#include <tl/optional.h>
#include <memory>

namespace Ichor::v1 {
    class OpenSSLCertificateStore final : public TLSCertificateStore {
    public:
        OpenSSLCertificateStore(X509_STORE_CTX* store, TLSCertificateStoreIdType id);
        ~OpenSSLCertificateStore() final;

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        [[nodiscard]] TLSCertificateStoreTypeType getType() const noexcept final;
#endif

        [[nodiscard]] tl::optional<std::unique_ptr<TLSCertificate>> getCurrentCertificate() const noexcept final;
    };
}
