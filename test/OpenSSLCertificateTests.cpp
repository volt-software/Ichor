#include "Common.h"
#include "SSLCerts.h"
#include <ichor/services/network/ssl/openssl/OpenSSLCertificate.h>
#include <ichor/stl/NeverAlwaysNull.h>
#include <chrono>
#include <ctime>
#include <memory>
#include <catch2/generators/catch_generators.hpp>

#include "Mocks/LoggerMock.h"

using namespace Ichor;
using namespace Ichor::v1;

TEST_CASE("OpenSSLCertificate basic properties") {
    const char *test_cert;
    uint64_t test_cert_size;
    std::string expectedSubjectC;
    std::string expectedSubjectST;
    std::string expectedSubjectL;
    std::string expectedSubjectO;
    std::string expectedSubjectCN;
    std::string expectedIssuerC;
    std::string expectedIssuerST;
    std::string expectedIssuerL;
    std::string expectedIssuerO;
    std::string expectedIssuerCN;
    std::string expectedSerial;
    std::string expectedSigAlg;

    int expectedBeforeYear;
    unsigned expectedBeforeMonth;
    unsigned expectedBeforeDay;
    int expectedBeforeHour;
    int expectedBeforeMinute;
    int expectedBeforeSecond;

    int expectedAfterYear;
    unsigned expectedAfterMonth;
    unsigned expectedAfterDay;
    int expectedAfterHour;
    int expectedAfterMinute;
    int expectedAfterSecond;

    std::tie(test_cert, test_cert_size, expectedSubjectC, expectedSubjectST, expectedSubjectL, expectedSubjectO, expectedSubjectCN, expectedIssuerC, expectedIssuerST, expectedIssuerL, expectedIssuerO, expectedIssuerCN, expectedSerial, expectedSigAlg,
             expectedBeforeYear, expectedBeforeMonth, expectedBeforeDay, expectedBeforeHour, expectedBeforeMinute, expectedBeforeSecond,
             expectedAfterYear, expectedAfterMonth, expectedAfterDay, expectedAfterHour, expectedAfterMinute, expectedAfterSecond) =
        GENERATE( table<const char*, uint64_t, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, int, unsigned, unsigned, int, int, int, int, unsigned, unsigned, int, int, int>({
            std::make_tuple(generic_test_cert, sizeof(generic_test_cert), "US", "CA", "Los AngelesO=BeastCN=www.example.com", "", "", "US", "CA", "Los AngelesO=BeastCN=www.example.com", "", "", std::string{"\xEF\x2F\x06\xEF\x22\xF1\xEC\x56", 8}, "RSA-SHA256", 2017, 5, 3, 18, 39, 12, 2044, 9, 18, 18, 39, 12),
            std::make_tuple(rsa2048_cert, sizeof(rsa2048_cert), "", "", "", "", "RSA-2048", "", "", "", "", "RSA-2048", std::string{"\x28\x1A\x3F\xF9\x95\x90\x40\xD6\x4B\xDE\xCF\x63\x7B\xFB\x4D\x85\x49\xBD\x27\xBF", 20}, "RSA-SHA256", 2025, 8, 23, 16, 8, 5, 3024, 12, 24, 16, 8, 5),
            std::make_tuple(rsa3072_cert, sizeof(rsa3072_cert), "", "", "", "", "RSA-3072", "", "", "", "", "RSA-3072", std::string{"\x46\xA2\x5E\x18\x66\x6F\x72\xAC\x35\xF3\x9F\xF1\x88\xB7\x01\xA6\x44\xB6\xE8\x0B", 20}, "RSA-SHA256", 2025, 8, 23, 16, 9, 37, 3024, 12, 24, 16, 9, 37),
            std::make_tuple(rsa4096_cert, sizeof(rsa4096_cert), "", "", "", "", "RSA-4096", "", "", "", "", "RSA-4096", std::string{"\x67\x11\x29\xA9\x92\x36\x32\xD9\x98\x0D\x9F\x23\x7A\x10\x1B\x4C\xC8\xE5\xB0\x88", 20}, "RSA-SHA256", 2025, 8, 23, 16, 11, 8, 3024, 12, 24, 16, 11, 8),
            std::make_tuple(rsa2048_sha1_cert, sizeof(rsa2048_sha1_cert), "", "", "", "", "RSA-SHA1", "", "", "", "", "RSA-SHA1", std::string{"\x19\x5E\x32\x45\x2F\x35\x18\x32\x1C\xC5\x4C\x43\x35\xD6\x98\xF4\xAC\x39\xA6\x4A", 20}, "RSA-SHA1", 2025, 8, 23, 16, 12, 18, 3024, 12, 24, 16, 12, 18),
            std::make_tuple(rsa1024_md5_cert, sizeof(rsa1024_md5_cert), "", "", "", "", "RSA-MD5", "", "", "", "", "RSA-MD5", std::string{"\x20\x3B\xA1\x16\x68\x4F\x09\x0B\xF4\x2A\x2B\xC0\x74\xFD\x79\xD7\xCF\xA3\xC9\xD5", 20}, "RSA-MD5", 2025, 8, 23, 16, 13, 49, 3024, 12, 24, 16, 13, 49),
            std::make_tuple(ed25519_cert, sizeof(ed25519_cert), "", "", "", "", "Ed25519", "", "", "", "", "Ed25519", std::string{"\x02\x80\x73\xF0\x0D\xB7\x24\x66\xA1\x2B\x0B\xDA\xAC\xD3\xB4\x71\xD7\xF6\xA2\x3A", 20}, "ED25519", 2025, 8, 23, 16, 14, 51, 3024, 12, 24, 16, 14, 51),
            std::make_tuple(ed448_cert, sizeof(ed448_cert), "", "", "", "", "Ed448", "", "", "", "", "Ed448", std::string{"\x29\x72\xDE\x79\xFA\x32\xC7\x3A\x11\xA8\x0F\xB2\x38\xE0\x46\x5D\x82\x85\x4C\xF1", 20}, "ED448", 2025, 8, 23, 16, 17, 29, 3024, 12, 24, 16, 17, 29),
            std::make_tuple(ecdsa_p256_cert, sizeof(ecdsa_p256_cert), "", "", "", "", "ECDSA-P256", "", "", "", "", "ECDSA-P256", std::string{"\x42\xCB\x6B\xAF\x0D\x4D\x39\x8D\x7B\x0B\xDD\xDA\xBF\x73\xFC\xF6\x51\x5F\xF7\x3A", 20}, "ecdsa-with-SHA256", 2025, 8, 23, 16, 19, 7, 3024, 12, 24, 16, 19, 7),
            std::make_tuple(ecdsa_p384_cert, sizeof(ecdsa_p384_cert), "", "", "", "", "ECDSA-P384", "", "", "", "", "ECDSA-P384", std::string{"\x1F\x82\xE0\x18\x9A\x05\x56\x9E\xE2\x40\x34\xC2\x05\x58\xF2\x1A\xAA\x85\x71\x72", 20}, "ecdsa-with-SHA256", 2025, 8, 23, 16, 21, 2, 3024, 12, 24, 16, 21, 2),
            std::make_tuple(ecdsa_p521_cert, sizeof(ecdsa_p521_cert), "", "", "", "", "ECDSA-P521", "", "", "", "", "ECDSA-P521", std::string{"\x72\xB4\x86\x18\x33\x63\x95\xB7\x3F\x51\x66\xC9\x32\x18\x13\x0D\x68\xAB\x87\x21", 20}, "ecdsa-with-SHA256", 2025, 8, 23, 16, 23, 9, 3024, 12, 24, 16, 23, 9),
            std::make_tuple(ecdsa_secp256k1_cert, sizeof(ecdsa_secp256k1_cert), "", "", "", "", "ECDSA-secp256k1", "", "", "", "", "ECDSA-secp256k1", std::string{"\x3B\x8C\x81\x75\x1A\xCE\x9E\x56\x85\x9A\xC1\x26\xB3\x32\x28\x5D\x56\x72\x97\xD0", 20}, "ecdsa-with-SHA256", 2025, 8, 23, 16, 27, 33, 3024, 12, 24, 16, 27, 33),
            std::make_tuple(ecdsa_bp256r1_cert, sizeof(ecdsa_bp256r1_cert), "", "", "", "", "ECDSA-brainpoolP256r1", "", "", "", "", "ECDSA-brainpoolP256r1", std::string{"\x3B\x09\xD6\xF6\xF6\x90\x89\x66\xD4\x80\x09\xFB\xF6\xAE\x09\x74\x6B\x50\xAD\xBD", 20}, "ecdsa-with-SHA256", 2025, 8, 23, 16, 29, 7, 3024, 12, 24, 16, 29, 7),
            std::make_tuple(ecdsa_bp384r1_cert, sizeof(ecdsa_bp384r1_cert), "", "", "", "", "ECDSA-brainpoolP384r1", "", "", "", "", "ECDSA-brainpoolP384r1", std::string{"\x2D\x98\x1E\x54\x5C\x9B\x66\x63\x3F\xA5\x66\x25\xFB\x9B\xA3\xC2\x2B\x8E\x66\xC4", 20}, "ecdsa-with-SHA256", 2025, 8, 23, 16, 30, 32, 3024, 12, 24, 16, 30, 32),
            std::make_tuple(ecdsa_bp512r1_cert, sizeof(ecdsa_bp512r1_cert), "", "", "", "", "ECDSA-brainpoolP512r1", "", "", "", "", "ECDSA-brainpoolP512r1", std::string{"\x58\x84\xCA\xDA\x8C\xA2\xB7\x5B\x05\xF5\xC9\xBE\x26\x9C\xAA\xF3\x43\x4E\xDD\x13", 20}, "ecdsa-with-SHA256", 2025, 8, 23, 16, 32, 8, 3024, 12, 24, 16, 32, 8),
            std::make_tuple(rsa2048_san_cert, sizeof(rsa2048_san_cert), "", "", "", "", "localhost", "", "", "", "", "localhost", std::string{"\x6A\x48\x12\x1E\xFA\x0F\x64\xD7\xF8\x51\x03\xC4\x6C\xE9\x1B\xA9\x4E\xB3\xC8\xFA", 20}, "RSA-SHA256", 2025, 8, 23, 16, 33, 16, 3024, 12, 24, 16, 33, 16),
            std::make_tuple(ecdsa_p256_eku_srv_cert, sizeof(ecdsa_p256_eku_srv_cert), "", "", "", "", "tls-server", "", "", "", "", "tls-server", std::string{"\x30\x93\x8A\x0A\x55\x45\x60\xE7\x94\x92\x34\x10\x52\xFF\x46\x9C\xB6\x8E\x49\x5D", 20}, "ecdsa-with-SHA256", 2025, 8, 23, 16, 48, 7, 3024, 12, 24, 16, 48, 7),
            std::make_tuple(ed25519_eku_cli_cert, sizeof(ed25519_eku_cli_cert), "", "", "", "", "tls-client", "", "", "", "", "tls-client", std::string{"\x38\xA6\xC1\x92\x49\x9A\x9E\xCE\x3A\xFE\x1F\x68\x76\x4C\x11\x99\x17\x9B\x06\x2A", 20}, "ED25519", 2025, 8, 23, 16, 50, 4, 3024, 12, 24, 16, 50, 4),
            std::make_tuple(rsa2048_eku_codesign_cert, sizeof(rsa2048_eku_codesign_cert), "", "", "", "", "CodeSign-Test", "", "", "", "", "CodeSign-Test", std::string{"\x7F\x6B\xB9\x4F\x53\xE2\x25\x2A\x03\x3E\x7E\x6B\xB3\xCB\x9C\x45\xCF\x34\x54\xB1s", 20}, "RSA-SHA256", 2025, 8, 23, 16, 51, 26, 3024, 12, 24, 16, 51, 26),
        } ) );

    fmt::println("testing cert {} {} {}", expectedSubjectC, expectedSubjectCN, (void*)test_cert);

    Ichor::Detail::DependencyLifecycleManager<LoggerMock, ILogger> logger{{}};

    auto cert = OpenSSLCertificate::makeOpenSSLCertificate(&logger.getService(), test_cert, test_cert_size - 1, TLSCertificateIdType{1});
    REQUIRE(cert);

    //SECTION("subject and issuer names") {
        auto subject = (*cert).getCommonNameViews(true);
        REQUIRE(subject.c == expectedSubjectC);
        REQUIRE(subject.st == expectedSubjectST);
        REQUIRE(subject.l == expectedSubjectL);
        REQUIRE(subject.o == expectedSubjectO);
        REQUIRE(subject.cn == expectedSubjectCN);
        REQUIRE(subject.ou.empty());
        REQUIRE(subject.dc.empty());
        REQUIRE(subject.email.empty());

        auto issuer = cert->getIssuerNameViews(true);
        REQUIRE(issuer.c == expectedIssuerC);
        REQUIRE(issuer.st == expectedIssuerST);
        REQUIRE(issuer.l == expectedIssuerL);
        REQUIRE(issuer.o == expectedIssuerO);
        REQUIRE(issuer.cn == expectedIssuerCN);
        REQUIRE(issuer.ou.empty());
        REQUIRE(issuer.dc.empty());
        REQUIRE(issuer.email.empty());
    // }

    // SECTION("serial number and version") {
        auto serial = cert->getSerialNumber();
        REQUIRE(serial == expectedSerial);
        REQUIRE(cert->getVersion() == 2);
    // }

    // SECTION("validity and signature") {
        auto validity = cert->getValidity();
        REQUIRE(validity.has_value());
        REQUIRE(validity->notAfter > validity->notBefore);

        std::chrono::year_month_day ymdNotBefore = std::chrono::floor<std::chrono::days>(validity->notBefore);
        std::chrono::year_month_day ymdNotAfter = std::chrono::floor<std::chrono::days>(validity->notAfter);

        REQUIRE(int(ymdNotBefore.year()) == expectedBeforeYear);
        REQUIRE(unsigned(ymdNotBefore.month()) == expectedBeforeMonth);
        REQUIRE(unsigned(ymdNotBefore.day()) == expectedBeforeDay);

        REQUIRE(int(ymdNotAfter.year()) == expectedAfterYear);
        REQUIRE(unsigned(ymdNotAfter.month()) == expectedAfterMonth);
        REQUIRE(unsigned(ymdNotAfter.day()) == expectedAfterDay);

        std::chrono::hh_mm_ss hmsNotBefore(validity->notBefore - std::chrono::floor<std::chrono::days>(validity->notBefore));
        std::chrono::hh_mm_ss hmsNotAfter(validity->notAfter - std::chrono::floor<std::chrono::days>(validity->notAfter));

        REQUIRE(hmsNotBefore.hours().count() == expectedBeforeHour);
        REQUIRE(hmsNotBefore.minutes().count() == expectedBeforeMinute);
        REQUIRE(hmsNotBefore.seconds().count() == expectedBeforeSecond);

        REQUIRE(hmsNotAfter.hours().count() == expectedAfterHour);
        REQUIRE(hmsNotAfter.minutes().count() == expectedAfterMinute);
        REQUIRE(hmsNotAfter.seconds().count() == expectedAfterSecond);

        auto signature = cert->getSignature();
        REQUIRE_FALSE(signature.empty());
        auto sigAlg = cert->getSignatureAlgorithm();
        REQUIRE(sigAlg == expectedSigAlg);
    // }

    // SECTION("public key") {
        auto pk = cert->getPublicKey();
        REQUIRE_FALSE(pk.empty());
    // }
}


TEST_CASE("OpenSSLCertificate incorrect certificates") {
    const char *test_cert;
    uint64_t test_cert_size;
    std::string certName;

    std::tie(test_cert, test_cert_size, certName) =
        GENERATE( table<const char*, uint64_t, std::string>({
            std::make_tuple(rsa2048_no_begin_cert, sizeof(rsa2048_no_begin_cert), NAMEOF(rsa2048_no_begin_cert)),
            std::make_tuple(rsa2048_no_end_cert, sizeof(rsa2048_no_end_cert), NAMEOF(rsa2048_no_end_cert)),
            std::make_tuple(rsa2048_bad_base64_cert, sizeof(rsa2048_bad_base64_cert), NAMEOF(rsa2048_bad_base64_cert)),
            std::make_tuple(rsa2048_truncated_cert, sizeof(rsa2048_truncated_cert), NAMEOF(rsa2048_truncated_cert)),
            std::make_tuple(rsa2048_invalid_der_header_cert, sizeof(rsa2048_invalid_der_header_cert), NAMEOF(rsa2048_invalid_der_header_cert)),
            std::make_tuple(rsa2048_invalid_der_header_length_cert, sizeof(rsa2048_invalid_der_header_length_cert), NAMEOF(rsa2048_invalid_der_header_length_cert)),
            std::make_tuple(rsa2048_invalid_der_header_length2_cert, sizeof(rsa2048_invalid_der_header_length2_cert), NAMEOF(rsa2048_invalid_der_header_length2_cert)),
            std::make_tuple(rsa2048_corrupted_oid_cert, sizeof(rsa2048_corrupted_oid_cert), NAMEOF(rsa2048_corrupted_oid_cert)),
            std::make_tuple(rsa2048_random_corruption_cert, sizeof(rsa2048_random_corruption_cert), NAMEOF(rsa2048_random_corruption_cert)),
        } ) );

    fmt::println("testing cert {}", certName);

    Ichor::Detail::DependencyLifecycleManager<LoggerMock, ILogger> logger{{}};

    auto cert = OpenSSLCertificate::makeOpenSSLCertificate(&logger.getService(), test_cert, test_cert_size - 1, TLSCertificateIdType{1});
    REQUIRE(!cert);
}


TEST_CASE("OpenSSLCertificate incorrect combo certificates") {
    const char *test_cert;
    uint64_t test_cert_size;
    std::string certName;

    std::tie(test_cert, test_cert_size, certName) =
        GENERATE( table<const char*, uint64_t, std::string>({
            std::make_tuple(rsa2048_combo_good_then_bad_cert, sizeof(rsa2048_combo_good_then_bad_cert), NAMEOF(rsa2048_combo_good_then_bad_cert)),
        } ) );

    fmt::println("testing cert {}", certName);

    Ichor::Detail::DependencyLifecycleManager<LoggerMock, ILogger> logger{{}};

    auto cert = OpenSSLCertificate::makeOpenSSLCertificate(&logger.getService(), test_cert, test_cert_size - 1, TLSCertificateIdType{1});
    REQUIRE(cert); // only one cert is actually read, the second is effectively ignored.
}
