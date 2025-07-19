#include <ichor/services/network/ssl/OpenSSLService.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>

namespace {
    struct OpenSSLContext final : public Ichor::v1::TLSContext {
        ~OpenSSLContext() final {
            if(_ctx != nullptr) {
                ::BIO_free(read_bio);
                ::SSL_free(static_cast<SSL *>(_ctx));
                _ctx = nullptr;
            }
        }

        SSL* getHandle() {
            return static_cast<SSL *>(_ctx);
        }

        ::BIO *write_bio{};
        ::BIO *read_bio{};
    };
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::v1::OpenSSLService::start() {
    BIO_nwrite0()
    co_return {};
}

Ichor::Task<> Ichor::v1::OpenSSLService::stop() {
    co_return;
}

tl::expected<Ichor::v1::TLSContext, Ichor::v1::TLSContextError> Ichor::v1::OpenSSLService::createTLSContext() {
    auto ctx_handle = ::SSL_CTX_new(SSLv23_client_method());

    if(ctx_handle == nullptr) {
        return tl::unexpected(TLSContextError::UNKNOWN);
    }

    SSL_CTX_set_min_proto_version(ctx_handle, TLS1_3_VERSION);

    auto ssl_handle = ::SSL_new(ctx_handle);

    OpenSSLContext ctx{ssl_handle};
    ::BIO_new_bio_pair(&ctx.write_bio, 0, &ctx.read_bio, 0);
    ::SSL_set_bio(ctx.getHandle(), ctx.read_bio, ctx.write_bio);

    return ctx;
}

tl::expected<void, Ichor::v1::TLSContextError> Ichor::v1::OpenSSLService::closeTLSContext(TLSContext &) {
}

tl::expected<Ichor::v1::TLSConnection, Ichor::v1::TLSConnectionError> Ichor::v1::OpenSSLService::createTLSConnection(TLSContext &) {
}

tl::expected<void, bool> Ichor::v1::OpenSSLService::TLSWrite(TLSContext &, std::string_view) {
}

tl::expected<void, bool> Ichor::v1::OpenSSLService::TLSRead(TLSContext &, std::string_view) {
}
