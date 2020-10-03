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
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};

        virtual std::vector<std::shared_ptr<spdlog::sinks::sink>>& getSinks() = 0;
    protected:
        ~ISpdlogSharedService() override = default;
    };

    class SpdlogSharedService final : public ISpdlogSharedService, public Service {
    public:
        bool start() final;
        bool stop() final;

        std::vector<std::shared_ptr<spdlog::sinks::sink>>& getSinks() final;

    private:
        std::vector<std::shared_ptr<spdlog::sinks::sink>> _sinks;
    };
}

#endif