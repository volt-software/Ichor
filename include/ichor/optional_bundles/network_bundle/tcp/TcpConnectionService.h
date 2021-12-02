#pragma once

#include <ichor/optional_bundles/network_bundle/IConnectionService.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/optional_bundles/timer_bundle/TimerService.h>

namespace Ichor {
    class TcpConnectionService final : public IConnectionService, public Service<TcpConnectionService> {
    public:
        TcpConnectionService(DependencyRegister &reg, Properties props, DependencyManager *mng);
        ~TcpConnectionService() final = default;

        bool start() final;
        bool stop() final;

        void addDependencyInstance(ILogger *logger, IService *isvc);
        void removeDependencyInstance(ILogger *logger, IService *isvc);

        bool send(std::vector<uint8_t, Ichor::PolymorphicAllocator<uint8_t>>&& msg) final;
        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        int _socket;
        int _attempts;
        uint64_t _priority;
        bool _quit;
        ILogger *_logger{nullptr};
        Timer* _timerManager{nullptr};
    };
}