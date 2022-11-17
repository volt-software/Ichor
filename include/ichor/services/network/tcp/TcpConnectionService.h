#pragma once

#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32) && !defined(__APPLE__)) || defined(__CYGWIN__)

#include <ichor/services/network/IConnectionService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/TimerService.h>

namespace Ichor {
    class TcpConnectionService final : public IConnectionService, public Service<TcpConnectionService> {
    public:
        TcpConnectionService(DependencyRegister &reg, Properties props, DependencyManager *mng);
        ~TcpConnectionService() final = default;

        uint64_t sendAsync(std::vector<uint8_t>&& msg) final;
        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        StartBehaviour start() final;
        StartBehaviour stop() final;

        void addDependencyInstance(ILogger *logger, IService *isvc);
        void removeDependencyInstance(ILogger *logger, IService *isvc);

        friend DependencyRegister;

        int _socket;
        int _attempts;
        uint64_t _priority;
        uint64_t _msgIdCounter;
        bool _quit;
        ILogger *_logger{nullptr};
        Timer* _timerManager{nullptr};
    };
}

#endif
