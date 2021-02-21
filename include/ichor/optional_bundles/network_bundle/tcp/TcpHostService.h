#pragma once

#include <ichor/optional_bundles/network_bundle/IHostService.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/optional_bundles/network_bundle/tcp/TcpConnectionService.h>
#include <thread>

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
        TcpHostService(DependencyRegister &reg, IchorProperties props, DependencyManager *mng);
        ~TcpHostService() final = default;

        bool start() final;
        bool stop() final;

        void addDependencyInstance(ILogger *logger);
        void removeDependencyInstance(ILogger *logger);

        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

        Generator<bool> handleEvent(NewSocketEvent const * const evt);

    private:
        int _socket;
        int _bindFd;
        std::atomic<uint64_t> _priority;
        std::atomic<bool> _quit;
        std::thread _listenThread;
        ILogger *_logger{nullptr};
        std::vector<TcpConnectionService*> _connections;
        std::unique_ptr<EventHandlerRegistration, Deleter> _newSocketEventHandlerRegistration{nullptr};
    };
}