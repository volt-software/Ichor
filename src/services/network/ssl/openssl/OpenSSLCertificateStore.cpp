#include <ichor/services/network/ssl/openssl/OpenSSLCertificateStore.h>
#include <ichor/services/network/ssl/openssl/OpenSSLCertificate.h>
#include <fmt/base.h>

using namespace Ichor::v1;

OpenSSLCertificateStore::OpenSSLCertificateStore(X509_STORE_CTX* store, TLSCertificateStoreIdType id) : TLSCertificateStore(store, id) {}

OpenSSLCertificateStore::~OpenSSLCertificateStore() = default;

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
TLSCertificateStoreTypeType OpenSSLCertificateStore::getType() const noexcept {
    return TLSCertificateStoreTypeType{1};
}
#endif

tl::optional<std::unique_ptr<TLSCertificate>> OpenSSLCertificateStore::getCurrentCertificate() const noexcept {
#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
    if(_ctx == nullptr) [[unlikely]] {
        fmt::println("fatal error, store context is nullptr, please file a bug in Ichor.");
        std::terminate();
    }
#endif
    auto cert = X509_STORE_CTX_get_current_cert(static_cast<X509_STORE_CTX*>(_ctx));

    if(cert == nullptr) {
        return {};
    }

    return std::make_unique<OpenSSLCertificate>(cert, TLSCertificateIdType{1});
}
