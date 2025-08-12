#include <ichor/services/network/ssl/OpenSSLService.h>
#include <ichor/ScopeGuard.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/x509_vfy.h>

#if defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
#define REFCOUNT_PARAM uint64_t &serviceContextRefCount
#define REFCOUNT_INITIALISER , _serviceContextRefCount(serviceContextRefCount)
#define REFCOUNT_MEMBER uint64_t &_serviceContextRefCount;
#define REFCOUNT_INC _serviceContextRefCount++;
#define REFCOUNT_DEC _serviceContextRefCount--;
#else
#define REFCOUNT_PARAM [[maybe_unused]] uint64_t &serviceContextRefCount
#define REFCOUNT_INITIALISER
#define REFCOUNT_MEMBER
#define REFCOUNT_INC
#define REFCOUNT_DEC
#endif

namespace Ichor::v1 {
    struct OpenSSLCertificate final : TLSCertificate {
        OpenSSLCertificate(NeverNull<X509>* store, Ichor::v1::TLSCertificateIdType id) : TLSCertificate(store, id) {}

        ~OpenSSLCertificate() final = default;

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        [[nodiscard]] TLSCertificateTypeType getType() const noexcept final {
            return TLSCertificateTypeType{1};
        }
#endif

        [[nodiscard]] std::string_view getCommonName() const noexcept final {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
            if(_ctx == nullptr) {
                fmt::println("fatal error, certificate context is nullptr, please file a bug in Ichor.");
                std::terminate();
            }
#endif
            X509_NAME *snName = X509_get_subject_name(static_cast<X509*>(_ctx.get()));

            int idx = X509_NAME_get_index_by_NID(snName, NID_commonName, -1);
            if(idx < 0) {
                return {};
            }

            X509_NAME_ENTRY *e = X509_NAME_get_entry(snName, idx);
            if (!e) {
                return {};
            }

            ASN1_STRING *s = X509_NAME_ENTRY_get_data(e);
            if (!s) {
                return {};
            }

            return std::string_view{reinterpret_cast<const char *>(ASN1_STRING_get0_data(s)), static_cast<long unsigned int>(ASN1_STRING_length(s))};
        }
    };

    struct OpenSSLCertificateStore final : TLSCertificateStore {
        OpenSSLCertificateStore(X509_STORE_CTX* store, Ichor::v1::TLSCertificateStoreIdType id) : TLSCertificateStore(store, id) {}

        ~OpenSSLCertificateStore() final = default;

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        [[nodiscard]] TLSCertificateStoreTypeType getType() const noexcept final {
            return TLSCertificateStoreTypeType{1};
        }
#endif

        [[nodiscard]] tl::optional<std::unique_ptr<TLSCertificate>> getCurrentCertificate() const noexcept final {
            return {};
        }
    };

    struct OpenSSLContext final : Ichor::v1::TLSContext {
        OpenSSLContext(SSL_CTX* ctx, Ichor::v1::TLSContextIdType id, REFCOUNT_PARAM, OpenSSLService &svc) : TLSContext(ctx, id) REFCOUNT_INITIALISER, _svc(svc) {
            REFCOUNT_INC
        }

        ~OpenSSLContext() final {
            if(_ctx != nullptr) {
                ::SSL_CTX_free(static_cast<SSL_CTX *>(_ctx));
                _ctx = nullptr;
            }
            REFCOUNT_DEC
        }

        SSL_CTX* getHandle() const {
            return static_cast<SSL_CTX *>(_ctx);
        }

        ILogger* getLogger() const {
            return _svc.getLogger();
        }

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        [[nodiscard]] TLSContextTypeType getType() const noexcept final {
            return TLSContextTypeType{1};
        }
#endif

        std::function<bool(const TLSCertificateStore &)> certificateVerifyCallback{};
        REFCOUNT_MEMBER
        OpenSSLService &_svc;
    };
}

namespace {

    struct OpenSSLConnection final : Ichor::v1::TLSConnection {
        OpenSSLConnection(SSL* ctx, Ichor::v1::TLSConnectionIdType id) : TLSConnection(ctx, id) {}

        ~OpenSSLConnection() final {
            if(_ctx != nullptr) {
                ::BIO_free(internalBio);
                ::BIO_free(externalBio);
                ::SSL_free(static_cast<SSL *>(_ctx));
                _ctx = nullptr;
            }
        }

        SSL* getHandle() const {
            return static_cast<SSL *>(_ctx);
        }

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        [[nodiscard]] Ichor::v1::TLSConnectionTypeType getType() const noexcept final {
            return Ichor::v1::TLSConnectionTypeType{1};
        }
#endif

        ::BIO *internalBio{};
        ::BIO *externalBio{};
    };

    struct NullStruct {};

    int SSLContextVerifyCertificateCallback(X509_STORE_CTX *storeCtx, void *userArg) {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        if(storeCtx == nullptr) {
            fmt::println("fatal error, store context is nullptr, please file a bug in Ichor.");
            std::terminate();
        }
        if(userArg == nullptr) {
            fmt::println("fatal error, user arg is nullptr, please file a bug in Ichor.");
            std::terminate();
        }
#endif

        int ret = X509_verify_cert(storeCtx);
        if(ret == 1) {
            return 1;
        }

        int err = X509_STORE_CTX_get_error(storeCtx);
        const char* err_msg = X509_verify_cert_error_string(err);

        X509* cert = X509_STORE_CTX_get_current_cert(storeCtx);
        char subject[256] = "<unknown>";
        if (cert) {
            X509_NAME_oneline(X509_get_subject_name(cert), subject, sizeof(subject));
        }
        auto sslContext = static_cast<Ichor::v1::OpenSSLContext*>(userArg);

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        if(sslContext->getType() != Ichor::v1::TLSContextTypeType{1}) [[unlikely]] {
            fmt::println("TLSConnection type expected {} got {}", 1, sslContext->getType().value);
            std::terminate();
        }
#endif
        auto logger = sslContext->getLogger();
        ICHOR_LOG_DEBUG(logger, "cert default verify failed, using custom callback: {} (subject: {})", err_msg, subject);

        fmt::println("cert verify failed: {} (subject: {}) ", err_msg, subject);

        if(sslContext->certificateVerifyCallback) {
            return sslContext->certificateVerifyCallback(Ichor::v1::OpenSSLCertificateStore{storeCtx, Ichor::v1::TLSCertificateStoreIdType{1}}); // TODO maybe id here is not necessary (and who is responsible for the counter??)
        }

        return 0;
    }
}

Ichor::v1::OpenSSLService::OpenSSLService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
    reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::v1::OpenSSLService::start() {
    co_return {};
}

Ichor::Task<> Ichor::v1::OpenSSLService::stop() {
    if(_currentlyActiveContexts != 0) {
        ICHOR_LOG_ERROR(_logger, "Memory leak detected and potentially dangerous situation, stopping with {} active contexts. Did you destroy all unique_ptrs<TLSContext> before stopping this service?", _currentlyActiveContexts);
    }
    co_return;
}

void Ichor::v1::OpenSSLService::addDependencyInstance(ILogger &logger, IService &isvc) {
    _logger = &logger;
}

void Ichor::v1::OpenSSLService::removeDependencyInstance(ILogger &logger, IService &isvc) {
    _logger = nullptr;
}

tl::expected<std::unique_ptr<Ichor::v1::TLSContext>, Ichor::v1::TLSContextError> Ichor::v1::OpenSSLService::createServerTLSContext(std::vector<uint8_t> cert, std::vector<uint8_t> key, TLSCreateContextOptions) {
    auto ctx_handle = ::SSL_CTX_new(SSLv23_server_method());

    if(ctx_handle == nullptr) {
        printAllSslErrors(NullStruct{});
        return tl::unexpected(TLSContextError::UNKNOWN);
    }

    SSL_CTX_set_min_proto_version(ctx_handle, TLS1_3_VERSION);
    BIO *cert_bio = ::BIO_new_mem_buf(cert.data(), cert.size());

    if(cert_bio == nullptr) {
        printAllSslErrors(NullStruct{});
        return tl::unexpected(TLSContextError::UNKNOWN);
    }

    ScopeGuard sgCertBio{[cert_bio] {
        ::BIO_free(cert_bio);
    }};
    X509* certx509 = ::PEM_read_bio_X509(cert_bio, nullptr, 0, nullptr);

    if(certx509 == nullptr) {
        printAllSslErrors(NullStruct{});
        return tl::unexpected(TLSContextError::UNKNOWN);
    }

    ScopeGuard sgCertx509{[certx509] {
        ::X509_free(certx509);
    }};
    auto ret = ::SSL_CTX_use_certificate(ctx_handle, certx509);

    if(ret != 1) {
        printAllSslErrors(NullStruct{});
        return tl::unexpected(TLSContextError::UNKNOWN);
    }

    BIO *key_bio = ::BIO_new_mem_buf(key.data(), key.size());

    if(key_bio == nullptr) {
        printAllSslErrors(NullStruct{});
        return tl::unexpected(TLSContextError::UNKNOWN);
    }

    ScopeGuard sgKeyBio{[key_bio] {
        ::BIO_free(key_bio);
    }};

    EVP_PKEY* pkey = ::PEM_read_bio_PrivateKey(key_bio, nullptr, 0, nullptr);

    if(pkey == nullptr) {
        printAllSslErrors(NullStruct{});
        return tl::unexpected(TLSContextError::UNKNOWN);
    }

    ScopeGuard sgPkey{[pkey] {
        ::EVP_PKEY_free(pkey);
    }};
    ret = ::SSL_CTX_use_PrivateKey(ctx_handle, pkey);

    if(ret != 1) {
        printAllSslErrors(NullStruct{});
        return tl::unexpected(TLSContextError::UNKNOWN);
    }

    ret = ::SSL_CTX_check_private_key(ctx_handle);

    if(ret != 1) {
        printAllSslErrors(NullStruct{});
        return tl::unexpected(TLSContextError::UNKNOWN);
    }

    ++_contextIdCounter;
    return std::make_unique<OpenSSLContext>(ctx_handle, _contextIdCounter, _currentlyActiveContexts, *this);
}

tl::expected<std::unique_ptr<Ichor::v1::TLSContext>, Ichor::v1::TLSContextError> Ichor::v1::OpenSSLService::createClientTLSContext(TLSCreateContextOptions opts) {
    auto ctx_handle = ::SSL_CTX_new(SSLv23_client_method());

    if(ctx_handle == nullptr) {
        return tl::unexpected(TLSContextError::UNKNOWN);
    }

    SSL_CTX_set_min_proto_version(ctx_handle, TLS1_3_VERSION);

    ++_contextIdCounter;
    auto tlsContext = std::make_unique<OpenSSLContext>(ctx_handle, _contextIdCounter, _currentlyActiveContexts, *this);

    if(opts.allowUnknownCertificates) {
        ::SSL_CTX_set_verify(ctx_handle, SSL_VERIFY_NONE, nullptr);
    } else {
        ::SSL_CTX_set_verify(ctx_handle, SSL_VERIFY_PEER, nullptr);
        ::SSL_CTX_set_cert_verify_callback(ctx_handle, &SSLContextVerifyCertificateCallback, tlsContext.get());
    }

    return tlsContext;
}

tl::expected<std::unique_ptr<Ichor::v1::TLSConnection>, Ichor::v1::TLSConnectionError> Ichor::v1::OpenSSLService::createTLSConnection(TLSContext &ctx, TLSCreateConnectionOptions opts) {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
    if(ctx.getType() != TLSContextTypeType{1}) [[unlikely]] {
        ICHOR_LOG_ERROR(_logger, "TLSContext type expected {} got {}", 1, ctx.getType().value);
        std::terminate();
    }
#endif

    auto ssl_handle = ::SSL_new(static_cast<OpenSSLContext&>(ctx).getHandle());

    if(ssl_handle == nullptr) {
        printAllSslErrors(NullStruct{});
        return tl::unexpected(TLSConnectionError::UNKNOWN);
    }

    ++_connectionIdCounter;
    auto con = std::make_unique<OpenSSLConnection>(ssl_handle, _connectionIdCounter);
    int ret = ::BIO_new_bio_pair(&con->internalBio, 0, &con->externalBio, 0);

    if(ret != 1) {
        printAllSslErrors(*con.get());
        return tl::unexpected(TLSConnectionError::UNKNOWN);
    }

    ::SSL_set_bio(con->getHandle(), con->internalBio, con->internalBio);
    auto key = ::SSL_CTX_get0_privatekey(static_cast<OpenSSLContext&>(ctx).getHandle());
    if(key == nullptr) {
        ::SSL_set_connect_state(con->getHandle());
    } else {
        ::SSL_set_accept_state(con->getHandle());
    }

    return con;
}

tl::expected<void, bool> Ichor::v1::OpenSSLService::TLSWrite(TLSConnection &conn, const std::vector<uint8_t> &data) {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
    if(conn.getType() != TLSConnectionTypeType{1}) [[unlikely]] {
        ICHOR_LOG_ERROR(_logger, "TLSConnection type expected {} got {}", 1, conn.getType().value);
        std::terminate();
    }
#endif

    if(data.size() > OSSL_SSIZE_MAX) [[unlikely]] {
        ICHOR_LOG_ERROR(_logger, "Connection {} tried to send data of length {} with max {} supported.", conn.getId().value, data.size(), OSSL_SSIZE_MAX);
        return tl::unexpected(false);
    }

    OpenSSLConnection &sslConn = static_cast<OpenSSLConnection &>(conn);
    char *out{};
    int chunk = BIO_nwrite0(sslConn.externalBio, &out);

    if(chunk < data.size()) {
        ICHOR_LOG_ERROR(_logger, "Connection {} tried to send data of length {} with chunk {} supported.", conn.getId().value, data.size(), chunk);
        return tl::unexpected(false);
    }

    std::memcpy(out, data.data(), data.size());
    int actual = BIO_nwrite(sslConn.externalBio, &out, data.size());

    if(actual != data.size()) {
        ICHOR_LOG_ERROR(_logger, "Connection {} should have written {} but actually wrote {}", conn.getId().value, data.size(), actual);
        return tl::unexpected(false);
    }

    return {};
}

tl::expected<std::vector<uint8_t>, bool> Ichor::v1::OpenSSLService::TLSRead(TLSConnection &conn) {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
    if(conn.getType() != TLSConnectionTypeType{1}) [[unlikely]] {
        ICHOR_LOG_ERROR(_logger, "TLSConnection type expected {} got {}", 1, conn.getType().value);
        std::terminate();
    }
#endif

    auto &sslConn = static_cast<OpenSSLConnection &>(conn);

    char *buf{};
    int len = BIO_nread0(sslConn.externalBio, &buf);
    std::vector<uint8_t> ret;

    if(len <= 0) {
        ICHOR_LOG_DEBUG(_logger, "Connection {} no data to read.", conn.getId().value);
        return tl::unexpected(false);
    }

    ret.resize(len);
    std::memcpy(ret.data(), buf, len);
    int actual = BIO_nread(sslConn.externalBio, &buf, len);

    if(actual != len) {
        ICHOR_LOG_ERROR(_logger, "Connection {} should have read {} but actually read {}", conn.getId().value, len, actual);
        return tl::unexpected(false);
    }

    return ret;
}

void Ichor::v1::OpenSSLService::TLSDoHandshake(TLSConnection &conn) {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
    if(conn.getType() != TLSConnectionTypeType{1}) [[unlikely]] {
        ICHOR_LOG_ERROR(_logger, "TLSConnection type expected {} got {}", 1, conn.getType().value);
        std::terminate();
    }
#endif

    OpenSSLConnection &sslConn = static_cast<OpenSSLConnection &>(conn);

    int ret = ::SSL_do_handshake(sslConn.getHandle());

    if(ret == SSL_ERROR_WANT_WRITE || ret == SSL_ERROR_WANT_READ) {

    } else if(ret != SSL_ERROR_NONE) {
        printAllSslErrors(sslConn);
    }

}

template <typename TLSObjectT>
void Ichor::v1::OpenSSLService::printAllSslErrors(TLSObjectT const &obj) const {
    char buf[256];
    auto errCode = ERR_get_error();
    while(errCode != 0) {
        ERR_error_string_n(errCode, buf, sizeof(buf));
        if constexpr(std::is_same_v<TLSObjectT, OpenSSLContext>) {
            ICHOR_LOG_ERROR(_logger, "OpenSSL error {} for ctx {}: {}", errCode, obj.getId().value, buf);
        } else if constexpr (std::is_same_v<TLSObjectT, OpenSSLConnection>) {
            ICHOR_LOG_ERROR(_logger, "OpenSSL error {} for conn {}: {}", errCode, obj.getId().value, buf);
        } else if constexpr (std::is_same_v<TLSObjectT, NullStruct>) {
            ICHOR_LOG_ERROR(_logger, "OpenSSL error {}: {}", errCode, buf);
        } else {
            static_assert(false, "TLSObjectT has to be either OpenSSLContext, OpenSSLConnection or NullStruct");
        }
        errCode = ERR_get_error();
    }
}

Ichor::v1::ILogger * Ichor::v1::OpenSSLService::getLogger() const {
    return _logger;
}
