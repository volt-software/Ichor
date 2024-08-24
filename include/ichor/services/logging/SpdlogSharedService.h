#pragma once

#ifdef ICHOR_USE_SPDLOG

#include <vector>
#include <memory>
#include <ichor/dependency_management/AdvancedService.h>

namespace spdlog::sinks {
    class sink;
}

namespace Ichor {
    class ISpdlogSharedService {
    public:
        virtual std::vector<std::shared_ptr<spdlog::sinks::sink>> const& getSinks() const noexcept = 0;
    protected:
        ~ISpdlogSharedService() = default;
    };

    class SpdlogSharedService final : public ISpdlogSharedService, public AdvancedService<SpdlogSharedService> {
    public:
        SpdlogSharedService() = default;
        ~SpdlogSharedService() final = default;

        std::vector<std::shared_ptr<spdlog::sinks::sink>> const& getSinks() const noexcept final;
    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        friend DependencyRegister;

        std::vector<std::shared_ptr<spdlog::sinks::sink>> _sinks;
    };
}

#endif
