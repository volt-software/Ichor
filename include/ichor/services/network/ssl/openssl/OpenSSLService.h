#pragma once

#include <ichor/services/network/ISSL.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>

namespace Ichor::v1 {
    enum class TLSHandshakeStatus {
        WANT_READ,
        WANT_WRITE,
        NEITHER,
        UNKNOWN
    };

    class OpenSSLContext;

    class OpenSSLService final : public ISSL, public AdvancedService<OpenSSLService> {
    public:
        OpenSSLService(DependencyRegister &reg, Properties props);
        ~OpenSSLService() final = default;

        tl::expected<std::unique_ptr<TLSContext>, TLSContextError> createServerTLSContext(std::vector<uint8_t> cert, std::vector<uint8_t> key, TLSCreateContextOptions opts) final;
        tl::expected<std::unique_ptr<TLSContext>, TLSContextError> createClientTLSContext(TLSCreateContextOptions opts) final;

        tl::expected<std::unique_ptr<TLSConnection>, TLSConnectionError> createTLSConnection(TLSContext &, TLSCreateConnectionOptions) final;

        tl::expected<void, bool> TLSWrite(TLSConnection &, const std::vector<uint8_t> &) final;
        tl::expected<std::vector<uint8_t>, bool> TLSRead(TLSConnection &) final;

        TLSHandshakeStatus TLSDoHandshake(TLSConnection &conn);

    private:
        friend DependencyRegister;
        friend DependencyManager;
        friend OpenSSLContext;

        Task<tl::expected<void, StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(ILogger &logger, IService &isvc);
        void removeDependencyInstance(ILogger &logger, IService &isvc);

        template <typename TLSObjectT>
        void printAllSslErrors(TLSObjectT const &obj) const;

        ILogger* getLogger() const;

        ILogger *_logger{};
        TLSContextIdType _contextIdCounter{};
        TLSConnectionIdType _connectionIdCounter{};
        uint64_t _currentlyActiveContexts{};
    };
}
