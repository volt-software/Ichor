#include <ichor/services/network/ssl/openssl/OpenSSLContext.h>

using namespace Ichor::v1;

OpenSSLContext::OpenSSLContext(SSL_CTX* ctx, Ichor::v1::TLSContextIdType id, REFCOUNT_PARAM, OpenSSLService &svc) : TLSContext(ctx, id) REFCOUNT_INITIALISER, _svc(svc) {
    REFCOUNT_INC
}

OpenSSLContext::~OpenSSLContext() {
    if(_ctx != nullptr) {
        ::SSL_CTX_free(static_cast<SSL_CTX *>(_ctx));
        _ctx = nullptr;
    }
    REFCOUNT_DEC
}

SSL_CTX* OpenSSLContext::getHandle() const {
    return static_cast<SSL_CTX *>(_ctx);
}

ILogger* OpenSSLContext::getLogger() const {
    return _svc.getLogger();
}

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
TLSContextTypeType OpenSSLContext::getType() const noexcept {
    return TLSContextTypeType{1};
}
#endif
