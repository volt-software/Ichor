#include "Common.h"
#include <ichor/dependency_management/InternalService.h>
#include <ichor/services/network/ssl/openssl/OpenSSLService.h>
#include <ichor/services/network/ssl/openssl/OpenSSLCertificate.h>
#include <ichor/services/network/ssl/openssl/OpenSSLContext.h>
#include <ichor/services/network/ssl/openssl/OpenSSLConnection.h>
#include <ichor/dependency_management/InternalServiceLifecycleManager.h>
#include <fstream>
#include <catch2/generators/catch_generators.hpp>

#include "FakeLifecycleManager.h"
#include "Mocks/LoggerMock.h"
#include "Mocks/QueueMock.h"
#include "SSLCerts.h"

TEST_CASE("OpenSSL Handshake Tests - supported server certificate only") {
    Properties props{};
    QueueMock qm{};
    Ichor::Detail::InternalServiceLifecycleManager<IEventQueue> q{&qm};
    Ichor::Detail::DependencyLifecycleManager<LoggerMock, ILogger> logger{{}};
    Ichor::Detail::DependencyLifecycleManager<OpenSSLService, ISSL> sslService{std::move(props)};

    auto [test_cert, test_cert_size, test_private_key, test_private_key_size, certName, security_level] =
        GENERATE( table<const char*, uint64_t, const char*, uint64_t, std::string, TLSContextSecurityLevel>({
            std::make_tuple(rsa2048_cert, sizeof(rsa2048_cert), rsa2048_private_key, sizeof(rsa2048_private_key), NAMEOF(rsa2048_cert), TLSContextSecurityLevel::DEFAULT),
            std::make_tuple(rsa3072_cert, sizeof(rsa3072_cert), rsa3072_private_key, sizeof(rsa3072_private_key), NAMEOF(rsa3072_cert), TLSContextSecurityLevel::DEFAULT),
            std::make_tuple(rsa4096_cert, sizeof(rsa4096_cert), rsa4096_private_key, sizeof(rsa4096_private_key), NAMEOF(rsa4096_cert), TLSContextSecurityLevel::DEFAULT),
            std::make_tuple(rsa2048_sha1_cert, sizeof(rsa2048_sha1_cert), rsa2048_sha1_private_key, sizeof(rsa2048_sha1_private_key), NAMEOF(rsa2048_sha1_cert), TLSContextSecurityLevel::DEFAULT),
            std::make_tuple(rsa1024_md5_cert, sizeof(rsa1024_md5_cert), rsa1024_md5_private_key, sizeof(rsa1024_md5_private_key), NAMEOF(rsa1024_md5_cert), TLSContextSecurityLevel::WEAKER_THAN_DEFAULT_IF_AVAILABLE_DO_NOT_USE_UNLESS_YOU_KNOW_WHAT_YOURE_DOING),
            std::make_tuple(ed25519_cert, sizeof(ed25519_cert), ed25519_private_key, sizeof(ed25519_private_key), NAMEOF(ed25519_cert), TLSContextSecurityLevel::DEFAULT),
            std::make_tuple(ed448_cert, sizeof(ed448_cert), ed448_private_key, sizeof(ed448_private_key), NAMEOF(ed448_cert), TLSContextSecurityLevel::DEFAULT),
            std::make_tuple(ecdsa_p256_cert, sizeof(ecdsa_p256_cert), ecdsa_p256_private_key, sizeof(ecdsa_p256_private_key), NAMEOF(ecdsa_p256_cert), TLSContextSecurityLevel::DEFAULT),
            std::make_tuple(ecdsa_p384_cert, sizeof(ecdsa_p384_cert), ecdsa_p384_private_key, sizeof(ecdsa_p384_private_key), NAMEOF(ecdsa_p384_cert), TLSContextSecurityLevel::DEFAULT),
            std::make_tuple(ecdsa_p521_cert, sizeof(ecdsa_p521_cert), ecdsa_p512_private_key, sizeof(ecdsa_p512_private_key), NAMEOF(ecdsa_p521_cert), TLSContextSecurityLevel::DEFAULT),

            // TLS 1.3 doesn't support these
            // std::make_tuple(ecdsa_secp256k1_cert, sizeof(ecdsa_secp256k1_cert), ecdsa_secp256k1_private_key, sizeof(ecdsa_secp256k1_private_key), NAMEOF(ecdsa_secp256k1_cert), TLSContextSecurityLevel::DEFAULT),
            // std::make_tuple(ecdsa_bp256r1_cert, sizeof(ecdsa_bp256r1_cert), ecdsa_bp256r1_private_key, sizeof(ecdsa_bp256r1_private_key), NAMEOF(ecdsa_bp256r1_cert), TLSContextSecurityLevel::DEFAULT),
            // std::make_tuple(ecdsa_bp384r1_cert, sizeof(ecdsa_bp384r1_cert), ecdsa_bp384r1_private_key, sizeof(ecdsa_bp384r1_private_key), NAMEOF(ecdsa_bp384r1_cert), TLSContextSecurityLevel::DEFAULT),
            // std::make_tuple(ecdsa_bp512r1_cert, sizeof(ecdsa_bp512r1_cert), ecdsa_bp512r1_private_key, sizeof(ecdsa_bp512r1_private_key), NAMEOF(ecdsa_bp512r1_cert), TLSContextSecurityLevel::DEFAULT),

            std::make_tuple(rsa2048_san_cert, sizeof(rsa2048_san_cert), rsa2048_san_private_key, sizeof(rsa2048_san_private_key), NAMEOF(rsa2048_san_cert), TLSContextSecurityLevel::DEFAULT),
            std::make_tuple(ecdsa_p256_eku_srv_cert, sizeof(ecdsa_p256_eku_srv_cert), ecdsa_p256_eku_srv_private_key, sizeof(ecdsa_p256_eku_srv_private_key), NAMEOF(ecdsa_p256_eku_srv_cert), TLSContextSecurityLevel::DEFAULT),
            std::make_tuple(ed25519_eku_cli_cert, sizeof(ed25519_eku_cli_cert), ed25519_eku_cli_private_key, sizeof(ed25519_eku_cli_private_key), NAMEOF(ed25519_eku_cli_cert), TLSContextSecurityLevel::DEFAULT),
            std::make_tuple(rsa2048_eku_codesign_cert, sizeof(rsa2048_eku_codesign_cert), rsa2048_eku_codesign_private_key, sizeof(rsa2048_eku_codesign_private_key), NAMEOF(rsa2048_eku_codesign_cert), TLSContextSecurityLevel::DEFAULT),
        } ) );

    fmt::println("testing cert {}", certName);

    TLSCreateContextOptions opts{.securityLevel = security_level};
    std::vector<uint8_t> cert, key;
    cert = std::vector<uint8_t>(test_cert, test_cert + test_cert_size);
    key = std::vector<uint8_t>(test_private_key, test_private_key + test_private_key_size);

    {
        auto ret = sslService.dependencyOnline(&q);
        REQUIRE(ret == StartBehaviour::DONE);
        ret = sslService.dependencyOnline(&logger);
        REQUIRE(ret == StartBehaviour::STARTED);
    }

    auto gen = sslService.start();
    auto it = gen.begin();
    REQUIRE(it.get_finished());

    /*SECTION("Basic handshake test with client and server")*/ {
        // Create server context
        auto serverContextOpt = sslService.getService().createServerTLSContext(cert, key, opts);
        REQUIRE(serverContextOpt);
        auto serverContext = std::move(*serverContextOpt);

        // Create client context
        auto clientContextOpt = sslService.getService().createClientTLSContext({.allowUnknownCertificates = true});
        REQUIRE(clientContextOpt);
        auto clientContext = std::move(*clientContextOpt);

        // Create server connection
        auto serverConnOpt = sslService.getService().createTLSConnection(*serverContext, {});
        REQUIRE(serverConnOpt);
        auto serverConn = std::move(*serverConnOpt);

        // Create client connection
        auto clientConnOpt = sslService.getService().createTLSConnection(*clientContext, {});
        REQUIRE(clientConnOpt);
        auto clientConn = std::move(*clientConnOpt);

        // Test initial handshake state
        auto status = sslService.getService().TLSDoHandshake(*serverConn);
        REQUIRE(status == Ichor::v1::TLSHandshakeStatus::WANT_READ);

        status = sslService.getService().TLSDoHandshake(*clientConn);
        REQUIRE(status == Ichor::v1::TLSHandshakeStatus::WANT_READ);


        auto data = sslService.getService().TLSRead(*serverConn);
        REQUIRE(!data);
        data = sslService.getService().TLSRead(*clientConn);
        REQUIRE(data);
        auto writeRet = sslService.getService().TLSWrite(*serverConn, *data);
        REQUIRE(writeRet);
        data = sslService.getService().TLSRead(*clientConn);
        REQUIRE(!data);

        status = sslService.getService().TLSDoHandshake(*serverConn);
        REQUIRE(status == Ichor::v1::TLSHandshakeStatus::WANT_READ);

        data = sslService.getService().TLSRead(*serverConn);
        REQUIRE(data);
        writeRet = sslService.getService().TLSWrite(*clientConn, *data);
        REQUIRE(writeRet);

        status = sslService.getService().TLSDoHandshake(*clientConn);
        REQUIRE(status == Ichor::v1::TLSHandshakeStatus::NEITHER);
    }

    /*SECTION("Verify callback test")*/ {
        // Create server context
        auto serverContextOpt = sslService.getService().createServerTLSContext(cert, key, opts);
        REQUIRE(serverContextOpt);
        auto serverContext = std::move(*serverContextOpt);

        // Create client context
        auto clientContextOpt = sslService.getService().createClientTLSContext({.certificateVerifyCallback = [] (const TLSCertificateStore &) {
            return true;
        }});
        REQUIRE(clientContextOpt);
        auto clientContext = std::move(*clientContextOpt);

        // Create server connection
        auto serverConnOpt = sslService.getService().createTLSConnection(*serverContext, {});
        REQUIRE(serverConnOpt);
        auto serverConn = std::move(*serverConnOpt);

        // Create client connection
        auto clientConnOpt = sslService.getService().createTLSConnection(*clientContext, {});
        REQUIRE(clientConnOpt);
        auto clientConn = std::move(*clientConnOpt);

        // Test initial handshake state
        auto status = sslService.getService().TLSDoHandshake(*serverConn);
        REQUIRE(status == Ichor::v1::TLSHandshakeStatus::WANT_READ);

        status = sslService.getService().TLSDoHandshake(*clientConn);
        REQUIRE(status == Ichor::v1::TLSHandshakeStatus::WANT_READ);


        auto data = sslService.getService().TLSRead(*serverConn);
        REQUIRE(!data);
        data = sslService.getService().TLSRead(*clientConn);
        REQUIRE(data);
        auto writeRet = sslService.getService().TLSWrite(*serverConn, *data);
        REQUIRE(writeRet);
        data = sslService.getService().TLSRead(*clientConn);
        REQUIRE(!data);

        status = sslService.getService().TLSDoHandshake(*serverConn);
        REQUIRE(status == Ichor::v1::TLSHandshakeStatus::WANT_READ);

        data = sslService.getService().TLSRead(*serverConn);
        REQUIRE(data);
        writeRet = sslService.getService().TLSWrite(*clientConn, *data);
        REQUIRE(writeRet);

        status = sslService.getService().TLSDoHandshake(*clientConn);
        REQUIRE(status == Ichor::v1::TLSHandshakeStatus::NEITHER);
    }

    /*SECTION("Handshake status enum values")*/ {
        // Create a minimal connection for testing
        auto clientContextOpt = sslService.getService().createClientTLSContext({.allowUnknownCertificates = true});
        REQUIRE(clientContextOpt);
        auto clientContext = std::move(*clientContextOpt);

        auto clientConnOpt = sslService.getService().createTLSConnection(*clientContext, {});
        REQUIRE(clientConnOpt);
        auto clientConn = std::move(*clientConnOpt);

        // Test that we get a valid status
        auto status = sslService.getService().TLSDoHandshake(*clientConn);
        REQUIRE((status == Ichor::v1::TLSHandshakeStatus::WANT_READ ||
                 status == Ichor::v1::TLSHandshakeStatus::WANT_WRITE ||
                 status == Ichor::v1::TLSHandshakeStatus::NEITHER ||
                 status == Ichor::v1::TLSHandshakeStatus::UNKNOWN));
    }
}

TEST_CASE("OpenSSL Handshake Tests - rejects weaker certificates") {
    Properties props{};
    QueueMock qm{};
    Ichor::Detail::InternalServiceLifecycleManager<IEventQueue> q{&qm};
    Ichor::Detail::DependencyLifecycleManager<LoggerMock, ILogger> logger{{}};
    Ichor::Detail::DependencyLifecycleManager<OpenSSLService, ISSL> sslService{std::move(props)};

    auto [test_cert, test_cert_size, test_private_key, test_private_key_size, certName, security_level] =
        GENERATE( table<const char*, uint64_t, const char*, uint64_t, std::string, TLSContextSecurityLevel>({
            std::make_tuple(rsa2048_cert, sizeof(rsa2048_cert), rsa2048_private_key, sizeof(rsa2048_private_key), NAMEOF(rsa2048_cert), TLSContextSecurityLevel::STRONGER_THAN_DEFAULT_IF_AVAILABLE),
            std::make_tuple(rsa2048_sha1_cert, sizeof(rsa2048_sha1_cert), rsa2048_sha1_private_key, sizeof(rsa2048_sha1_private_key), NAMEOF(rsa2048_sha1_cert), TLSContextSecurityLevel::STRONGER_THAN_DEFAULT_IF_AVAILABLE), // Should fail at openssl default, but doesn't with openssl 3.0.2 for some reason.
            std::make_tuple(rsa1024_md5_cert, sizeof(rsa1024_md5_cert), rsa1024_md5_private_key, sizeof(rsa1024_md5_private_key), NAMEOF(rsa1024_md5_cert), TLSContextSecurityLevel::DEFAULT),
        } ) );

    fmt::println("testing cert {}", certName);

    TLSCreateContextOptions opts{.securityLevel = security_level};
    std::vector<uint8_t> cert, key;
    cert = std::vector<uint8_t>(test_cert, test_cert + test_cert_size);
    key = std::vector<uint8_t>(test_private_key, test_private_key + test_private_key_size);

    {
        auto ret = sslService.dependencyOnline(&q);
        REQUIRE(ret == StartBehaviour::DONE);
        ret = sslService.dependencyOnline(&logger);
        REQUIRE(ret == StartBehaviour::STARTED);
    }

    auto gen = sslService.start();
    auto it = gen.begin();
    REQUIRE(it.get_finished());

    auto serverContextOpt = sslService.getService().createServerTLSContext(cert, key, opts);
    REQUIRE(!serverContextOpt);
}
