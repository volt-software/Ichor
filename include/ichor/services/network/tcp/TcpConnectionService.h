#pragma once

#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32) && !defined(__APPLE__)) || defined(__CYGWIN__)

#include <ichor/services/network/IConnectionService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>

namespace Ichor {

    /**
     * Service for managing a TCP connection
     *
     * Properties:
     * - "Address" std::string - What address to connect to (required if Socket is not present)
     * - "Port" uint16_t - What port to connect to (required if Socket is not present)
     * - "Socket" int - An existing socket to manage (required if Address/Port are not present)
     * - "Priority" uint64_t - Which priority to use for inserted events (default INTERNAL_EVENT_PRIORITY)
     * - "TimeoutSendUs" int64_t - Timeout in microseconds for send calls (default 250'000)
     * - "TimeoutRecvUs" int64_t - Timeout in microseconds for recv calls (default 250'000)
     */
    class TcpConnectionService final : public IConnectionService, public AdvancedService<TcpConnectionService> {
    public:
        TcpConnectionService(DependencyRegister &reg, Properties props);
        ~TcpConnectionService() final = default;

        Task<tl::expected<uint64_t, IOError>> sendAsync(std::vector<uint8_t>&& msg) final;
        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

        [[nodiscard]] bool isClient() const noexcept final;

        void setReceiveHandler(std::function<void(std::span<uint8_t const>)>) final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(ILogger &logger, IService &isvc);
        void removeDependencyInstance(ILogger &logger, IService &isvc);

        void addDependencyInstance(ITimerFactory &logger, IService &isvc);
        void removeDependencyInstance(ITimerFactory &logger, IService &isvc);

        void recvHandler();

        friend DependencyRegister;

        static uint64_t tcpConnId;
        int _socket;
        uint64_t _id;
        uint64_t _attempts;
        uint64_t _priority;
        uint64_t _msgIdCounter{};
        int64_t _sendTimeout{250'000};
        int64_t _recvTimeout{250'000};
        bool _quit;
        ILogger *_logger{};
        ITimerFactory *_timerFactory{};
        ITimer *_timer{};
        std::vector<std::vector<uint8_t>> _queuedMessages{};
        std::function<void(std::span<uint8_t const>)> _recvHandler;
    };
}

#endif
