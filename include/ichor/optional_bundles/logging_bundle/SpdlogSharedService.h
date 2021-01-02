#pragma once

#ifdef USE_SPDLOG

#include <vector>
#include <ichor/Service.h>

namespace spdlog::sinks {
    class sink;
}

namespace Ichor {
    class ISpdlogSharedService : virtual public IService {
    public:
        virtual std::vector<std::shared_ptr<spdlog::sinks::sink>> const& getSinks() noexcept = 0;
    protected:
        ~ISpdlogSharedService() override = default;
    };

    class SpdlogSharedService final : public ISpdlogSharedService, public Service<SpdlogSharedService> {
    public:
        bool start() final;
        bool stop() final;

        std::vector<std::shared_ptr<spdlog::sinks::sink>> const& getSinks() noexcept final;
    private:
        std::vector<std::shared_ptr<spdlog::sinks::sink>> _sinks;
    };
}

#endif