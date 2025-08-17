#pragma once

#include <ichor/services/network/ISSL.h>
#include <ichor/services/network/ssl/openssl/OpenSSLService.h>
#include <openssl/ssl.h>
#include <functional>

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
    class OpenSSLContext final : public TLSContext {
    public:
        OpenSSLContext(SSL_CTX* ctx, TLSContextIdType id, REFCOUNT_PARAM, OpenSSLService &svc);
        ~OpenSSLContext() final;

        SSL_CTX* getHandle() const;
        ILogger* getLogger() const;

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        [[nodiscard]] TLSContextTypeType getType() const noexcept final;
#endif

        std::function<bool(const TLSCertificateStore &)> certificateVerifyCallback{};

    private:
        REFCOUNT_MEMBER
        OpenSSLService &_svc;
    };
}
