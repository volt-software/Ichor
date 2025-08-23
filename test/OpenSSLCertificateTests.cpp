#include "Common.h"
#include <ichor/services/network/ssl/openssl/OpenSSLCertificate.h>
#include <ichor/stl/NeverAlwaysNull.h>
#include <openssl/pem.h>
#include <chrono>
#include <ctime>
#include <memory>

using namespace Ichor;
using namespace Ichor::v1;

static const char test_cert[] = R"(-----BEGIN CERTIFICATE-----
MIIDaDCCAlCgAwIBAgIJAO8vBu8i8exWMA0GCSqGSIb3DQEBCwUAMEkxCzAJBgNV
BAYTAlVTMQswCQYDVQQIDAJDQTEtMCsGA1UEBwwkTG9zIEFuZ2VsZXNPPUJlYXN0
Q049d3d3LmV4YW1wbGUuY29tMB4XDTE3MDUwMzE4MzkxMloXDTQ0MDkxODE4Mzkx
MlowSTELMAkGA1UEBhMCVVMxCzAJBgNVBAgMAkNBMS0wKwYDVQQHDCRMb3MgQW5n
ZWxlc089QmVhc3RDTj13d3cuZXhhbXBsZS5jb20wggEiMA0GCSqGSIb3DQEBAQUA
A4IBDwAwggEKAoIBAQDJ7BRKFO8fqmsEXw8v9YOVXyrQVsVbjSSGEs4Vzs4cJgcF
xqGitbnLIrOgiJpRAPLy5MNcAXE1strVGfdEf7xMYSZ/4wOrxUyVw/Ltgsft8m7b
Fu8TsCzO6XrxpnVtWk506YZ7ToTa5UjHfBi2+pWTxbpN12UhiZNUcrRsqTFW+6fO
9d7xm5wlaZG8cMdg0cO1bhkz45JSl3wWKIES7t3EfKePZbNlQ5hPy7Pd5JTmdGBp
yY8anC8u4LPbmgW0/U31PH0rRVfGcBbZsAoQw5Tc5dnb6N2GEIbq3ehSfdDHGnrv
enu2tOK9Qx6GEzXh3sekZkxcgh+NlIxCNxu//Dk9AgMBAAGjUzBRMB0GA1UdDgQW
BBTZh0N9Ne1OD7GBGJYz4PNESHuXezAfBgNVHSMEGDAWgBTZh0N9Ne1OD7GBGJYz
4PNESHuXezAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQCmTJVT
LH5Cru1vXtzb3N9dyolcVH82xFVwPewArchgq+CEkajOU9bnzCqvhM4CryBb4cUs
gqXWp85hAh55uBOqXb2yyESEleMCJEiVTwm/m26FdONvEGptsiCmF5Gxi0YRtn8N
V+KhrQaAyLrLdPYI7TrwAOisq2I1cD0mt+xgwuv/654Rl3IhOMx+fKWKJ9qLAiaE
fQyshjlPP9mYVxWOxqctUdQ8UnsUKKGEUcVrA08i1OAnVKlPFjKBvk+r7jpsTPcr
9pWXTO9JrYMML7d+XRSZA1n3856OqZDX4403+9FnXCvfcLZLLKTBvwwFgEFGpzjK
UEVbkhd5qstF6qWK
-----END CERTIFICATE-----
)";

TEST_CASE("OpenSSLCertificate basic properties") {
    BIO *bio = BIO_new_mem_buf(test_cert, static_cast<int>(sizeof(test_cert) - 1));
    REQUIRE(bio != nullptr);
    std::unique_ptr<X509, decltype(&X509_free)> x509{PEM_read_bio_X509(bio, nullptr, nullptr, nullptr), &X509_free};
    BIO_free(bio);
    REQUIRE(x509 != nullptr);

    OpenSSLCertificate cert{NeverNull<X509*>{x509.get()}, TLSCertificateIdType{1}};

    SECTION("subject and issuer names") {
        auto subject = cert.getCommonNameViews(true);
        REQUIRE(subject.c == "US");
        REQUIRE(subject.st == "CA");

        auto issuer = cert.getIssuerNameViews(true);
        REQUIRE(issuer.c == "US");
        REQUIRE(issuer.st == "CA");
    }

    SECTION("serial number and version") {
        auto serial = cert.getSerialNumber();
        const std::string expected_serial{"\xEF\x2F\x06\xEF\x22\xF1\xEC\x56", 8};
        REQUIRE(serial == expected_serial);
        REQUIRE(cert.getVersion() == 2);
    }

    SECTION("validity and signature") {
        auto validity = cert.getValidity();
        REQUIRE(validity.has_value());
        REQUIRE(validity->notAfter > validity->notBefore);

        std::chrono::year_month_day ymdNotBefore = std::chrono::floor<std::chrono::days>(validity->notBefore);
        std::chrono::year_month_day ymdNotAfter = std::chrono::floor<std::chrono::days>(validity->notAfter);

        REQUIRE(ymdNotBefore.year() == 2017y);
        REQUIRE(ymdNotBefore.month() == std::chrono::May);
        REQUIRE(ymdNotBefore.day() == 3d);

        REQUIRE(ymdNotAfter.year() == 2044y);
        REQUIRE(ymdNotAfter.month() == std::chrono::September);
        REQUIRE(ymdNotAfter.day() == 18d);

        std::chrono::hh_mm_ss hmsNotBefore(validity->notBefore - std::chrono::floor<std::chrono::days>(validity->notBefore));
        std::chrono::hh_mm_ss hmsNotAfter(validity->notAfter - std::chrono::floor<std::chrono::days>(validity->notAfter));

        REQUIRE(hmsNotBefore.hours().count() == 18);
        REQUIRE(hmsNotBefore.minutes().count() == 39);
        REQUIRE(hmsNotBefore.seconds().count() == 12);

        REQUIRE(hmsNotAfter.hours().count() == 18);
        REQUIRE(hmsNotAfter.minutes().count() == 39);
        REQUIRE(hmsNotAfter.seconds().count() == 12);

        auto signature = cert.getSignature();
        REQUIRE_FALSE(signature.empty());
        auto sigAlg = cert.getSignatureAlgorithm();
        REQUIRE(sigAlg == "RSA-SHA256");
    }

    SECTION("public key") {
        auto pk = cert.getPublicKey();
        REQUIRE_FALSE(pk.empty());
    }
}
