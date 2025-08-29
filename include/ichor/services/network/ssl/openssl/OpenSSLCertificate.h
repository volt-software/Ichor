#pragma once

#include <ichor/services/network/ISSL.h>
#include <ichor/services/logging/Logger.h>
#include <openssl/x509_vfy.h>
#include <ctime>
#include <vector>

namespace Ichor::v1 {
    enum class OpenSSLMakeCertificateError {
        OUT_OF_MEMORY,
        ERROR_WITH_CERTIFICATE
    };

    class OpenSSLCertificate final : public TLSCertificate {
    public:
        OpenSSLCertificate(NeverNull<X509*> store, TLSCertificateIdType id);
        OpenSSLCertificate(OpenSSLCertificate const &) = delete;
        OpenSSLCertificate(OpenSSLCertificate&&) noexcept;
        OpenSSLCertificate& operator=(OpenSSLCertificate const &) = delete;
        OpenSSLCertificate& operator=(OpenSSLCertificate&&) = delete;
        ~OpenSSLCertificate() final;

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        [[nodiscard]] TLSCertificateTypeType getType() const noexcept final;
#endif


        static tl::expected<OpenSSLCertificate, OpenSSLMakeCertificateError> makeOpenSSLCertificate(tl::optional<ILogger*> logger, NeverNull<const char *> data, uint64_t dataLength, TLSCertificateIdType id) noexcept;
        static std::string_view asn1StringAsView(const ASN1_STRING* s) noexcept;
        static std::string_view getFirstAttribute(NeverNull<X509_NAME*> name, int nid) noexcept;
        static std::vector<std::string_view> getAllAttributes(const X509_NAME* name, int nid) noexcept;
        static tl::optional<std::chrono::sys_seconds> convertASN1tmUTCtoTimepoint(tm &tm_utc) noexcept;

        [[nodiscard]] TLSCertificateNameViews getCommonNameViews(bool includeVectorViews) const noexcept final;
        [[nodiscard]] TLSCertificateNameViews getIssuerNameViews(bool includeVectorViews) const noexcept final;
        [[nodiscard]] std::string_view getSerialNumber() const noexcept final;
        [[nodiscard]] uint32_t getVersion() const noexcept final;
        [[nodiscard]] tl::optional<TLSValidity> getValidity() const noexcept final;
        [[nodiscard]] std::string_view getSignature() const noexcept final;
        [[nodiscard]] std::string_view getSignatureAlgorithm() const noexcept final;
        [[nodiscard]] std::string_view getPublicKey() const noexcept final;

    private:

        NeverNull<X509*> _ctx;
        bool _shouldFreeCtx{};
    };
}
