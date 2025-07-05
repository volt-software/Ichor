#pragma once

#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32) && !defined(__APPLE__)) || defined(__CYGWIN__)

#include <ichor/services/network/IConnectionService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/Concepts.h>

namespace Ichor::v1 {

    /**
     * Service for managing a TCP connection
     *
     * Properties:
     * - "Address" std::string - What address to connect to (required if Socket is not present)
     * - "Port" uint16_t - What port to connect to (required if Socket is not present)
     * - "Socket" int - An existing socket to manage (required if Address/Port are not present)
     * - "Priority" uint64_t - Which priority to use for inserted events (default INTERNAL_EVENT_PRIORITY)
     * - "TimeoutSendUs" int64_t - Timeout in microseconds for send calls (default 250'000)
     */
    template <typename InterfaceT> requires DerivedAny<InterfaceT, IConnectionService, IHostConnectionService, IClientConnectionService>
    class TcpConnectionService final : public InterfaceT, public AdvancedService<TcpConnectionService<InterfaceT>> {
    public:
        TcpConnectionService(DependencyRegister &reg, Properties props);
        ~TcpConnectionService() final = default;

        Task<tl::expected<void, IOError>> sendAsync(std::vector<uint8_t>&& msg) final;
        Task<tl::expected<void, IOError>> sendAsync(std::vector<std::vector<uint8_t>>&& msgs) final;
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

        int _socket;
        uint64_t _attempts;
        uint64_t _priority;
        int64_t _sendTimeout{250'000};
        bool _quit;
        ILogger *_logger{};
        ITimerFactory *_timerFactory{};
        ITimer *_timer{};
        std::vector<std::vector<uint8_t>> _queuedMessages{};
        std::function<void(std::span<uint8_t const>)> _recvHandler;
    };
}

#endif
