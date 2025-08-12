#pragma once

#include <cstdint>
#include <memory>
#include <tl/expected.h>
#include <ichor/stl/StrongTypedef.h>

#include "ichor/stl/NeverAlwaysNull.h"
#include "tl/optional.h"

namespace Ichor::v1 {
    struct TLSCertificateIdType : StrongTypedef<uint64_t, TLSCertificateIdType> {};
    struct TLSCertificateTypeType : StrongTypedef<uint64_t, TLSCertificateTypeType> {};
    struct TLSCertificateStoreIdType : StrongTypedef<uint64_t, TLSCertificateStoreIdType> {};
    struct TLSCertificateStoreTypeType : StrongTypedef<uint64_t, TLSCertificateStoreTypeType> {};
    struct TLSContextIdType : StrongTypedef<uint64_t, TLSContextIdType> {};
    struct TLSContextTypeType : StrongTypedef<uint64_t, TLSContextTypeType> {};
    struct TLSConnectionIdType : StrongTypedef<uint64_t, TLSConnectionIdType> {};
    struct TLSConnectionTypeType : StrongTypedef<uint64_t, TLSConnectionTypeType> {};

    struct TLSCertificate {
        TLSCertificate(NeverNull<void*> ctx, TLSCertificateIdType id) : _ctx{ctx}, _id{id} {}
        virtual ~TLSCertificate() = default;

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        [[nodiscard]] virtual TLSCertificateTypeType getType() const noexcept = 0;
#endif

        [[nodiscard]] virtual std::string_view getCommonName() const noexcept = 0;

    protected:
        NeverNull<void*> _ctx;
        TLSCertificateIdType _id{};

    };

    struct TLSCertificateStore {
        TLSCertificateStore(void *ctx, TLSCertificateStoreIdType id) : _ctx{ctx}, _id{id} {}
        virtual ~TLSCertificateStore() = default;

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        [[nodiscard]] virtual TLSCertificateStoreTypeType getType() const noexcept = 0;
#endif

        [[nodiscard]] virtual tl::optional<std::unique_ptr<TLSCertificate>> getCurrentCertificate() const noexcept = 0;

    protected:
        void *_ctx{};
        TLSCertificateStoreIdType _id{};
    };

    struct TLSContext {
        TLSContext(void *ctx, TLSContextIdType id) : _ctx{ctx}, _id{id} {}
        virtual ~TLSContext() = default;

        [[nodiscard]] TLSContextIdType getId() const {
            return _id;
        }

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        [[nodiscard]] virtual TLSContextTypeType getType() const noexcept = 0;
#endif

    protected:
        void *_ctx{};
        TLSContextIdType _id{};
    };

    struct TLSConnection {
        TLSConnection(void *ctx, TLSConnectionIdType id) : _ctx{ctx}, _id{id} {}
        virtual ~TLSConnection() = default;

        [[nodiscard]] TLSConnectionIdType getId() const {
            return _id;
        }

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        [[nodiscard]] virtual TLSConnectionTypeType getType() const noexcept = 0;
#endif

        void *_ctx{};
        TLSConnectionIdType _id{};
    };

    enum class TLSContextError {
        UNKNOWN
    };

    enum class TLSConnectionError {
        UNKNOWN
    };

    struct TLSCreateContextOptions {
        std::vector<uint8_t> trustedCertificates{};
        bool allowUnknownCertificates{};
    };

    struct TLSCreateConnectionOptions {
    };

    class ISSL {
    public:

        [[nodiscard]] virtual tl::expected<std::unique_ptr<TLSContext>, TLSContextError> createServerTLSContext(std::vector<uint8_t> cert, std::vector<uint8_t> key, TLSCreateContextOptions) = 0;
        [[nodiscard]] virtual tl::expected<std::unique_ptr<TLSContext>, TLSContextError> createClientTLSContext(TLSCreateContextOptions) = 0;
        [[nodiscard]] virtual tl::expected<std::unique_ptr<TLSConnection>, TLSConnectionError> createTLSConnection(TLSContext&, TLSCreateConnectionOptions) = 0;
        [[nodiscard]] virtual tl::expected<void, bool> TLSWrite(TLSConnection&, const std::vector<uint8_t> &) = 0;
        [[nodiscard]] virtual tl::expected<std::vector<uint8_t>, bool> TLSRead(TLSConnection&) = 0;

    protected:
        ~ISSL() = default;
    };
}
