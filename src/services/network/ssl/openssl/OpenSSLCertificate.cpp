#include <ichor/services/network/ssl/openssl/OpenSSLCertificate.h>
#include <fmt/base.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/evp.h>
#include <ctime>

using namespace Ichor::v1;

OpenSSLCertificate::OpenSSLCertificate(NeverNull<X509*> store, TLSCertificateIdType id) : TLSCertificate(store, id) {}

OpenSSLCertificate::~OpenSSLCertificate() = default;

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
TLSCertificateTypeType OpenSSLCertificate::getType() const noexcept {
    return TLSCertificateTypeType{1};
}
#endif

std::string_view OpenSSLCertificate::asn1StringAsView(const ASN1_STRING* s) {
    if(!s) {
        return {};
    }

    const unsigned char* p = ASN1_STRING_get0_data(s);
    int n = ASN1_STRING_length(s);

    if(!p || n <= 0) {
        return {};
    }

    return std::string_view{reinterpret_cast<const char*>(p), static_cast<long unsigned int>(n)};
}

std::string_view OpenSSLCertificate::getFirstAttribute(const X509_NAME* name, int nid) {
    int idx = X509_NAME_get_index_by_NID(name, nid, -1);

    if(idx < 0) {
        return {};
    }

    X509_NAME_ENTRY* e = X509_NAME_get_entry(name, idx);

    if(!e) {
        return {};
    }

    const ASN1_STRING* s = X509_NAME_ENTRY_get_data(e);

    return asn1StringAsView(s);
}

std::vector<std::string_view> OpenSSLCertificate::getAllAttributes(const X509_NAME* name, int nid) {
    std::vector<std::string_view> ret{};
    int last = -1;
    for(;;) {
        int idx = X509_NAME_get_index_by_NID(name, nid, last);

        if(idx < 0) {
            break;
        }

        last = idx;
        X509_NAME_ENTRY* e = X509_NAME_get_entry(name, idx);

        if(!e) {
            break;
        }

        const ASN1_STRING* s = X509_NAME_ENTRY_get_data(e);
        auto view = asn1StringAsView(s);

        if(!view.empty()) {
            ret.push_back(view);
        }
    }

    return ret;
}

tl::optional<std::chrono::system_clock::time_point> OpenSSLCertificate::convertASN1tmUTCtoTimepoint(tm &tm_utc) {
    const std::chrono::year  y{tm_utc.tm_year + 1900};
    const std::chrono::month m{static_cast<unsigned>(tm_utc.tm_mon + 1)};
    const std::chrono::day   d{static_cast<unsigned>(tm_utc.tm_mday)};

    if (!y.ok() || !m.ok() || !d.ok()) {
        return {};
    }

    // sys_days is a time_point on system_clock with day precision.
    const std::chrono::sys_days days = std::chrono::year_month_day{y, m, d};
    auto tp = days + std::chrono::hours{tm_utc.tm_hour}
    + std::chrono::minutes{tm_utc.tm_min}
    + std::chrono::seconds{tm_utc.tm_sec};

    return tp;
}

TLSCertificateNameViews OpenSSLCertificate::getCommonNameViews(bool includeVectorViews) const noexcept {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
    if(_ctx == nullptr) {
        fmt::println("fatal error, certificate context is nullptr, please file a bug in Ichor.");
        std::terminate();
    }
#endif

    X509_NAME *snName = X509_get_subject_name(static_cast<X509*>(_ctx.get()));

    TLSCertificateNameViews nameView{};
    nameView.c = getFirstAttribute(snName, NID_countryName);
    nameView.st = getFirstAttribute(snName, NID_stateOrProvinceName);
    nameView.l = getFirstAttribute(snName, NID_localityName);
    nameView.o = getFirstAttribute(snName, NID_organizationName);
    nameView.cn = getFirstAttribute(snName, NID_commonName);
    nameView.cn = getFirstAttribute(snName, NID_commonName);

    if(includeVectorViews) {
        nameView.ou = getAllAttributes(snName, NID_organizationalUnitName);
        nameView.dc = getAllAttributes(snName, NID_domainComponent);
        nameView.email = getAllAttributes(snName, NID_pkcs9_emailAddress);
    }

    return nameView;
}

TLSCertificateNameViews OpenSSLCertificate::getIssuerNameViews(bool includeVectorViews) const noexcept {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
    if(_ctx == nullptr) {
        fmt::println("fatal error, certificate context is nullptr, please file a bug in Ichor.");
        std::terminate();
    }
#endif

    X509_NAME *issName = X509_get_issuer_name(static_cast<X509*>(_ctx.get()));

    TLSCertificateNameViews nameView{};
    nameView.c = getFirstAttribute(issName, NID_countryName);
    nameView.st = getFirstAttribute(issName, NID_stateOrProvinceName);
    nameView.l = getFirstAttribute(issName, NID_localityName);
    nameView.o = getFirstAttribute(issName, NID_organizationName);
    nameView.cn = getFirstAttribute(issName, NID_commonName);
    nameView.cn = getFirstAttribute(issName, NID_commonName);

    if(includeVectorViews) {
        nameView.ou = getAllAttributes(issName, NID_organizationalUnitName);
        nameView.dc = getAllAttributes(issName, NID_domainComponent);
        nameView.email = getAllAttributes(issName, NID_pkcs9_emailAddress);
    }

    return nameView;
}

std::string_view OpenSSLCertificate::getSerialNumber() const noexcept {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
    if(_ctx == nullptr) {
        fmt::println("fatal error, certificate context is nullptr, please file a bug in Ichor.");
        std::terminate();
    }
#endif

    ASN1_INTEGER const *serial = X509_get0_serialNumber(static_cast<X509 *>(_ctx.get()));

    if(serial == nullptr) {
        return {};
    }

    const unsigned char* p = ASN1_STRING_get0_data(serial);
    int len = ASN1_STRING_length(serial);

    if(len <= 0 || !p) {
        return {};
    }

    // DER INTEGER is two’s complement; positive integers may have an extra 0x00
    // if the highest bit would otherwise be 1. With DER’s minimal-encoding rule,
    // at most one such 0x00 exists (and only when needed).
    if(len > 1 && p[0] == 0x00) {
        ++p; --len;
    }

    return std::string_view{reinterpret_cast<const char *>(p), static_cast<long unsigned int>(len)};
}

uint32_t OpenSSLCertificate::getVersion() const noexcept {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
    if(_ctx == nullptr) {
        fmt::println("fatal error, certificate context is nullptr, please file a bug in Ichor.");
        std::terminate();
    }
#endif

    long version = X509_get_version(static_cast<X509*>(_ctx.get()));

    if(version < 0) {
        return {};
    }

    return static_cast<uint32_t>(version);
}

tl::optional<TLSValidity> OpenSSLCertificate::getValidity() const noexcept {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
    if(_ctx == nullptr) {
        fmt::println("fatal error, certificate context is nullptr, please file a bug in Ichor.");
        std::terminate();
    }
#endif

    auto *notBefore = X509_get0_notBefore(static_cast<X509*>(_ctx.get()));
    auto *notAfter = X509_get0_notAfter(static_cast<X509*>(_ctx.get()));

    auto timepoint = [](const ASN1_TIME *tmCert) -> tl::optional<std::chrono::system_clock::time_point> {
        if(tmCert == nullptr) {
            return {};
        }
        tm tmUtc{};
        auto rc = ASN1_TIME_to_tm(tmCert, &tmUtc);
        if(rc == 0) {
            return {};
        }
        return convertASN1tmUTCtoTimepoint(tmUtc);
    };

    auto notBeforePoint = timepoint(notBefore);

    if(!notBeforePoint) {
        return {};
    }

    auto notAfterPoint = timepoint(notAfter);

    if(!notAfterPoint) {
        return {};
    }

    TLSValidity validity{};
    validity.notBefore = *notBeforePoint;
    validity.notAfter = *notAfterPoint;

    return validity;
}

std::string_view OpenSSLCertificate::getSignature() const noexcept {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
    if(_ctx == nullptr) {
        fmt::println("fatal error, certificate context is nullptr, please file a bug in Ichor.");
        std::terminate();
    }
#endif
    const ASN1_BIT_STRING *sig{};
    X509_get0_signature(&sig, nullptr, static_cast<X509*>(_ctx.get()));

    if(sig == nullptr) {
        return {};
    }

    return std::string_view{reinterpret_cast<const char *>(ASN1_STRING_get0_data(sig)), static_cast<long unsigned int>(ASN1_STRING_length(sig))};
}

std::string_view OpenSSLCertificate::getSignatureAlgorithm() const noexcept {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
    if(_ctx == nullptr) {
        fmt::println("fatal error, certificate context is nullptr, please file a bug in Ichor.");
        std::terminate();
    }
#endif
    const X509_ALGOR *alg{};
    X509_get0_signature(nullptr, &alg, static_cast<X509*>(_ctx.get()));

    if(alg == nullptr) {
        return {};
    }

    int nid = OBJ_obj2nid(alg->algorithm);
    const char *sn = OBJ_nid2sn(nid);

    if(sn == nullptr) {
        return {};
    }

    return std::string_view{sn};
}

std::string_view OpenSSLCertificate::getPublicKey() const noexcept {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
    if(_ctx == nullptr) {
        fmt::println("fatal error, certificate context is nullptr, please file a bug in Ichor.");
        std::terminate();
    }
#endif
    const ASN1_BIT_STRING *pk = X509_get0_pubkey_bitstr(static_cast<X509*>(_ctx.get()));

    if(pk == nullptr) {
        return {};
    }

    return std::string_view{reinterpret_cast<const char *>(ASN1_STRING_get0_data(pk)), static_cast<long unsigned int>(ASN1_STRING_length(pk))};
}
