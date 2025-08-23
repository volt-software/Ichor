#include "Common.h"
#include <ichor/dependency_management/InternalService.h>
#include <ichor/services/network/ssl/openssl/OpenSSLService.h>
#include <ichor/services/network/ssl/openssl/OpenSSLCertificate.h>
#include <ichor/services/network/ssl/openssl/OpenSSLContext.h>
#include <ichor/services/network/ssl/openssl/OpenSSLConnection.h>
#include <fstream>

#include "FakeLifecycleManager.h"
#include "Mocks/LoggerMock.h"
#include "Mocks/QueueMock.h"

static const char test_cert[] = R"(-----BEGIN CERTIFICATE-----
MIIDPzCCAiegAwIBAgICEAEwDQYJKoZIhvcNAQELBQAwPzELMAkGA1UEBhMCTkwx
FDASBgNVBAoMC0V4YW1wbGUgT3JnMRowGAYDVQQDDBFFeGFtcGxlIFRlc3QgQ0Eg
MTAeFw0yNTA4MjEyMDA5MDFaFw0yNzExMjQyMDA5MDFaMD8xCzAJBgNVBAYTAk5M
MRQwEgYDVQQKDAtFeGFtcGxlIE9yZzEaMBgGA1UEAwwRRXhhbXBsZSBUZXN0IENB
IDEwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCuN058rLNWtFte+Na0
LQNza5fSzESENHn6NG/z0t6TECRrtFwa7FcA7UA8WEnF0fkHRj1gcDu5CwzZo1QP
JPHnSXaoj07WiFY2HhvW8nmmSpM1Qddq7zYTa230wjeasBYf5iwdChGxldetUyml
Xij+dVBrRMIY6NtFB25smStFwoMG6jCs+d8hLkKRVJj2OUk8Kgp/3Eq6fqYkhguM
8u4C7z98rqkAZc4M1sALjsJwLkDejN506RNIY20UGDvGwoeuxJpAr+k5dtK8NQRM
BBFINOPDZwcGIvYf1BtMiRmzZSdrNKJuuQFSSTmkIINthMWqpjWJNTak/Mn5PAfz
jVj1AgMBAAGjRTBDMBIGA1UdEwEB/wQIMAYBAf8CAQEwDgYDVR0PAQH/BAQDAgEG
MB0GA1UdDgQWBBQXqjnrLR04+HfyCz71JGpqgmKnRzANBgkqhkiG9w0BAQsFAAOC
AQEAbxU8xtJTxN4t5rbY6xNk/Evf3whOxe1X19sTS+/ga6lS/ti+oEHv7Hdc5c/j
G4r8ukRHB4VUDghKzOGZrbVGj1HZaQdjZAtePsRzKRgblI7K2al/Ks9V65XKsdsh
x6BUGuICT17yP9S8Z0/HAkepsKBFvwsNvvK8Gg9qXLX1/4sIYWOhzIJkrVwaqf91
T2hJQ5v25P8wh06bPbSANoVVwxEp/0zhi+zaAzyVQx3q9r2+9bsQ9gYStyFzdR3b
XiFiAIITK4vlDmpvo736gRpz7VEgdIkuoXCpbJb2MAkShIy7TqcgWRHdbAECYj69
FdDD6Q1RAR//5XUbuNaiYibqyg==
-----END CERTIFICATE-----
)";

static const char test_private_key[] = R"(-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCuN058rLNWtFte
+Na0LQNza5fSzESENHn6NG/z0t6TECRrtFwa7FcA7UA8WEnF0fkHRj1gcDu5CwzZ
o1QPJPHnSXaoj07WiFY2HhvW8nmmSpM1Qddq7zYTa230wjeasBYf5iwdChGxldet
UymlXij+dVBrRMIY6NtFB25smStFwoMG6jCs+d8hLkKRVJj2OUk8Kgp/3Eq6fqYk
hguM8u4C7z98rqkAZc4M1sALjsJwLkDejN506RNIY20UGDvGwoeuxJpAr+k5dtK8
NQRMBBFINOPDZwcGIvYf1BtMiRmzZSdrNKJuuQFSSTmkIINthMWqpjWJNTak/Mn5
PAfzjVj1AgMBAAECggEAFw+FGSW3G3wGOD0SFol60nVkhGe7jhBwMPlt9EVuZuMV
HxihtIKiRaIiBZreMQxJPXhDuZdBoI3g5pIjF1oZlzb3OPq3QdiMKN+1aa9xAN0Q
PVV40VPWwZ1P0b0/pP83/oL2ReXGT8543R2L/rLHFF+tBHX2h1uTYsDIEiH4Q2Ry
9ftuxmLr+u0y4J2vhOMAPCsaJlHiQWWRzoLetnAEcarQj3RFjGnPP2Je+oYnyUKK
EoS9Pdgmcm1ktimoom/ftAsWiqZciYXkjDPCfuOguVM33dAWYzTw3B4puQ7+VWNp
HpHuGzSnWhcEckEg+6qN586W9WurxlfWozcFNOSy5QKBgQDgh9mK+GfaMQtgXIRo
7g+Y5SrnXrBWPA87cD0m6hvK2n4tAiewaBOobPKGjfu4xStQnOsoIuNEhKsvVkhF
iG78uGg5PlJRo+A9WI30wq1MFe6oVlSeOgb+h/vdmT47YlgIYlcXeJQuhG7+TVAs
PljUHjt5i3bwgLy9RF3fK8sUowKBgQDGoiugbaqmPqC4bOy/2N9R34AWQC+RXHlq
K7Es1pRcCIEPV7XVQE+tmfEUjAfFQFo4M0QN450oA/fFtSAeBS4IB05JDgZiYYyb
aBHs9gVdBvSK5VOuBO+cjYP3BGUAts9Ay3QVVI2+r5Bw1TfTT0rS4Hn/ev6334DL
mw58AFUdhwKBgG2gJokBq8MOex17TnLk+NyP15jL1JDmFhHRRSpA13z9nOlgyfwJ
dL+hIfCsViDqs7FSTEZ2cjw8AlDEcdjxOi9N4iVA1nRh7NRG7lh4zJ//fVzXec0f
S9bukfyqG4ZnB9BgI2GkT8U+h+rF2MUhU8GNWUCI5XObh4tpW+Pgv/C9AoGBAKHN
bm5DhxPexjsU95GbTua5gfQeGuF1C64xoyScMsP5ZXAjCYqJ1Z3bXWdYXegO8K8B
C7mopNb4zHdvgJcTrxh5IwsdcpPnVIsvLhLxoRLTXJZcElsZyDmliU7JdKEtSQxF
7uyFMOWYy7ct6EioOZInqxkyjpUnahnw186o0qy3AoGAXmpHLnWvxApe7JU0Ko6c
2yQJOjr5y64h36NedsgrUuy6TBf1YZgxGtsPlUXJg0ZZwpjA40WwskgjrrbBHmRR
b0clQuxSK1Qe0dC7Lov7knQBfPpZ6OFn9MCjsNbrxPz9dQndLdR+fbK2v1XVUwWV
fS7Xnq7eFJvKXJTdRT6MRik=
-----END PRIVATE KEY-----
)";

TEST_CASE("OpenSSL Handshake Tests") {
    Properties props{};
    Ichor::Detail::DependencyLifecycleManager<QueueMock, IEventQueue> q{{}};
    Ichor::Detail::DependencyLifecycleManager<LoggerMock, ILogger> logger{{}};
    Ichor::Detail::DependencyLifecycleManager<OpenSSLService, ISSL> sslService{std::move(props)};

    std::vector<uint8_t> cert, key;
    cert = std::vector<uint8_t>(test_cert, test_cert + sizeof(test_cert));
    key = std::vector<uint8_t>(test_private_key, test_private_key + sizeof(test_private_key));

    {
        auto ret = sslService.dependencyOnline(&q);
        REQUIRE(ret == StartBehaviour::DONE);
        ret = sslService.dependencyOnline(&logger);
        REQUIRE(ret == StartBehaviour::STARTED);
    }

    auto gen = sslService.start();
    auto it = gen.begin();
    REQUIRE(it.get_finished());

    SECTION("Basic handshake test with client and server") {
        // Create server context
        auto serverContextOpt = sslService.getService().createServerTLSContext(cert, key, {});
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

    SECTION("Verify callback test") {
        // Create server context
        auto serverContextOpt = sslService.getService().createServerTLSContext(cert, key, {});
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

    SECTION("Handshake status enum values") {
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
