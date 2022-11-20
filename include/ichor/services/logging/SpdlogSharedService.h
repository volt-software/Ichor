#pragma once

#ifdef ICHOR_USE_SPDLOG

#include <vector>
#include <ichor/Service.h>

namespace spdlog::sinks {
    class sink;
}

namespace Ichor {
    class ISpdlogSharedService {
    public:
        virtual std::vector<std::shared_ptr<spdlog::sinks::sink>> const& getSinks() noexcept = 0;
    protected:
        ~ISpdlogSharedService() = default;
    };

    class SpdlogSharedService final : public ISpdlogSharedService, public Service<SpdlogSharedService> {
    public:
        SpdlogSharedService() = default;
        ~SpdlogSharedService() final = default;

        std::vector<std::shared_ptr<spdlog::sinks::sink>> const& getSinks() noexcept final;
    private:
        AsyncGenerator<void> start() final;
        AsyncGenerator<void> stop() final;

        friend DependencyRegister;

        std::vector<std::shared_ptr<spdlog::sinks::sink>> _sinks;
    };
}

#endif