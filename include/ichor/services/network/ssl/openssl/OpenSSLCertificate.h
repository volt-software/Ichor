#pragma once

#include <ichor/services/network/ISSL.h>
#include <openssl/x509_vfy.h>
#include <ctime>
#include <vector>

namespace Ichor::v1 {
    class OpenSSLCertificate final : public TLSCertificate {
    public:
        OpenSSLCertificate(NeverNull<X509*> store, TLSCertificateIdType id);
        ~OpenSSLCertificate() final;

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        [[nodiscard]] TLSCertificateTypeType getType() const noexcept final;
#endif

        static std::string_view asn1StringAsView(const ASN1_STRING* s);
        static std::string_view getFirstAttribute(const X509_NAME* name, int nid);
        static std::vector<std::string_view> getAllAttributes(const X509_NAME* name, int nid);
        static tl::optional<std::chrono::system_clock::time_point> convertASN1tmUTCtoTimepoint(tm &tm_utc);

        [[nodiscard]] TLSCertificateNameViews getCommonNameViews(bool includeVectorViews) const noexcept final;
        [[nodiscard]] TLSCertificateNameViews getIssuerNameViews(bool includeVectorViews) const noexcept final;
        [[nodiscard]] std::string_view getSerialNumber() const noexcept final;
        [[nodiscard]] uint32_t getVersion() const noexcept final;
        [[nodiscard]] tl::optional<TLSValidity> getValidity() const noexcept final;
        [[nodiscard]] std::string_view getSignature() const noexcept final;
        [[nodiscard]] std::string_view getSignatureAlgorithm() const noexcept final;
        [[nodiscard]] std::string_view getPublicKey() const noexcept final;
    };
}
