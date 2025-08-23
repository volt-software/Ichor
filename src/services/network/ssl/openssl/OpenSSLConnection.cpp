#include <ichor/services/network/ssl/openssl/OpenSSLConnection.h>

using namespace Ichor::v1;

OpenSSLConnection::OpenSSLConnection(SSL* ctx, TLSConnectionIdType id) : TLSConnection(ctx, id) {}

OpenSSLConnection::~OpenSSLConnection() {
    if(_ctx != nullptr) {
        ::BIO_free(externalBio);
        ::SSL_free(static_cast<SSL *>(_ctx));
        _ctx = nullptr;
    }
}

SSL* OpenSSLConnection::getHandle() const {
    return static_cast<SSL *>(_ctx);
}

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
TLSConnectionTypeType OpenSSLConnection::getType() const noexcept {
    return TLSConnectionTypeType{1};
}
#endif
