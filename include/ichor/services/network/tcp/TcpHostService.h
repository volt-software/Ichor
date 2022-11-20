#pragma once

#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32) && !defined(__APPLE__)) || defined(__CYGWIN__)

#include <ichor/services/network/IHostService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/network/tcp/TcpConnectionService.h>
#include <ichor/services/timer/TimerService.h>

namespace Ichor {
    struct NewSocketEvent final : public Ichor::Event {
        NewSocketEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, int _socket) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority), socket(_socket) {}
        ~NewSocketEvent() final = default;

        int socket;
        static constexpr uint64_t TYPE = Ichor::typeNameHash<NewSocketEvent>();
        static constexpr std::string_view NAME = Ichor::typeName<NewSocketEvent>();
    };

    class TcpHostService final : public IHostService, public Service<TcpHostService> {
    public:
        TcpHostService(DependencyRegister &reg, Properties props, DependencyManager *mng);
        ~TcpHostService() final = default;

        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        AsyncGenerator<void> start() final;
        AsyncGenerator<void> stop() final;

        void addDependencyInstance(ILogger *logger, IService *isvc);
        void removeDependencyInstance(ILogger *logger, IService *isvc);

        AsyncGenerator<IchorBehaviour> handleEvent(NewSocketEvent const &evt);

        friend DependencyRegister;
        friend DependencyManager;

        int _socket;
        int _bindFd;
        uint64_t _priority;
        bool _quit;
        ILogger *_logger{nullptr};
        Timer* _timerManager{nullptr};
        std::vector<TcpConnectionService*> _connections;
        EventHandlerRegistration _newSocketEventHandlerRegistration{};
    };
}

#endif
