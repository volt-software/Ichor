#pragma once

#include <ichor/services/network/ISSL.h>
#include <ichor/dependency_management/AdvancedService.h>

namespace Ichor::v1 {

    class OpenSSLService final : public ISSL, public AdvancedService<OpenSSLService> {
        OpenSSLService(DependencyRegister &reg, Properties props);
        ~OpenSSLService() final = default;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

    public:
        tl::expected<TLSContext, TLSContextError> createTLSContext() final;

        tl::expected<TLSConnection, TLSConnectionError> createTLSConnection(TLSContext &) final;

        tl::expected<void, bool> TLSWrite(TLSContext &, std::string_view) final;

        tl::expected<void, bool> TLSRead(TLSContext &, std::string_view) final;

    private:
        friend DependencyRegister;
    };
}
