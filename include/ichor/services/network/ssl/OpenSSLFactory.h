#pragma once

#include <ichor/DependencyManager.h>

namespace Ichor::v1 {
    class OpenSSLFactory final : public AdvancedService<OpenSSLFactory> {
    public:
        OpenSSLFactory(DependencyRegister &reg, Properties properties) : AdvancedService<OpenSSLFactory>(std::move(properties)) {
        }
        ~OpenSSLFactory() final = default;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;

        Task<void> stop() final;


        friend DependencyRegister;
        friend DependencyManager;

    };
}
