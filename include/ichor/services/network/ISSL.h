#pragma once

#include <chrono>
#include <memory>
#include <string_view>
#include <functional>
#include <tl/expected.h>
#include <tl/optional.h>
#include <ichor/stl/StrongTypedef.h>
#include <ichor/stl/NeverAlwaysNull.h>

namespace Ichor::v1 {
    struct TLSCertificateIdType : StrongTypedef<uint64_t, TLSCertificateIdType> {};
    struct TLSCertificateTypeType : StrongTypedef<uint64_t, TLSCertificateTypeType> {};
    struct TLSCertificateStoreIdType : StrongTypedef<uint64_t, TLSCertificateStoreIdType> {};
    struct TLSCertificateStoreTypeType : StrongTypedef<uint64_t, TLSCertificateStoreTypeType> {};
    struct TLSContextIdType : StrongTypedef<uint64_t, TLSContextIdType> {};
    struct TLSContextTypeType : StrongTypedef<uint64_t, TLSContextTypeType> {};
    struct TLSConnectionIdType : StrongTypedef<uint64_t, TLSConnectionIdType> {};
    struct TLSConnectionTypeType : StrongTypedef<uint64_t, TLSConnectionTypeType> {};

    struct TLSCertificateNameViews {
        std::string_view c;
        std::string_view st;
        std::string_view l;
        std::string_view o;
        std::string_view cn;
        std::vector<std::string_view> ou;
        std::vector<std::string_view> dc;
        std::vector<std::string_view> email;
    };

    struct TLSValidity {
        std::chrono::system_clock::time_point notBefore;
        std::chrono::system_clock::time_point notAfter;
    };

    struct TLSCertificate {
        virtual ~TLSCertificate() = default;

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        [[nodiscard]] virtual TLSCertificateTypeType getType() const noexcept = 0;
#endif

        [[nodiscard]] virtual TLSCertificateNameViews getCommonNameViews(bool includeVectorViews) const noexcept = 0;
        [[nodiscard]] virtual TLSCertificateNameViews getIssuerNameViews(bool includeVectorViews) const noexcept = 0;
        [[nodiscard]] virtual std::string_view getSerialNumber() const noexcept = 0;
        [[nodiscard]] virtual uint32_t getVersion() const noexcept = 0;
        [[nodiscard]] virtual tl::optional<TLSValidity> getValidity() const noexcept = 0;
        [[nodiscard]] virtual std::string_view getSignature() const noexcept = 0;
        [[nodiscard]] virtual std::string_view getSignatureAlgorithm() const noexcept = 0;
        [[nodiscard]] virtual std::string_view getPublicKey() const noexcept = 0;

    protected:
        TLSCertificate(NeverNull<void*> ctx, TLSCertificateIdType id) : _ctx{ctx}, _id{id} {}

        NeverNull<void*> _ctx;
        TLSCertificateIdType _id{};
    };

    struct TLSCertificateStore {
        virtual ~TLSCertificateStore() = default;

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        [[nodiscard]] virtual TLSCertificateStoreTypeType getType() const noexcept = 0;
#endif

        [[nodiscard]] virtual tl::optional<std::unique_ptr<TLSCertificate>> getCurrentCertificate() const noexcept = 0;

    protected:
        TLSCertificateStore(void *ctx, TLSCertificateStoreIdType id) : _ctx{ctx}, _id{id} {}

        void *_ctx{};
        TLSCertificateStoreIdType _id{};
    };

    struct TLSContext {
        virtual ~TLSContext() = default;

        [[nodiscard]] TLSContextIdType getId() const {
            return _id;
        }

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        [[nodiscard]] virtual TLSContextTypeType getType() const noexcept = 0;
#endif

    protected:
        TLSContext(void *ctx, TLSContextIdType id) : _ctx{ctx}, _id{id} {}

        void *_ctx{};
        TLSContextIdType _id{};
    };

    struct TLSConnection {
        virtual ~TLSConnection() = default;

        [[nodiscard]] TLSConnectionIdType getId() const {
            return _id;
        }

#if defined(ICHOR_USE_HARDENING) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
        [[nodiscard]] virtual TLSConnectionTypeType getType() const noexcept = 0;
#endif

    protected:
        TLSConnection(void *ctx, TLSConnectionIdType id) : _ctx{ctx}, _id{id} {}

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
        std::function<bool(const TLSCertificateStore &)> certificateVerifyCallback{};
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
