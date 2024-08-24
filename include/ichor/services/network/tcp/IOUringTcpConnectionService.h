#pragma once

#ifndef ICHOR_USE_LIBURING
#error "Ichor has not been compiled with io_uring support"
#endif

#include <ichor/services/network/IConnectionService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <vector>

namespace Ichor {

    /**
     * Service for managing a TCP connection
     *
     * Properties:
     * - "Address" std::string - What address to connect to (required if Socket is not present)
     * - "Port" uint16_t - What port to connect to (required if Socket is not present)
     * - "Socket" int - An existing socket to manage (required if Address/Port are not present)
     * - "RecvFunction" std::function<void(std::span<uint8_t>)> - Function to call when socket receives data (required)
     * - "RecvBufferSize" uint64_t - Size of receive buffer (default 2048, only if kernel version is < 6.0.0)
     * - "TimeoutSendUs" int64_t - Timeout in microseconds for send calls (default 250'000)
     * - "TimeoutRecvUs" int64_t - Timeout in microseconds for recv calls (default 250'000)
     */
    class IOUringTcpConnectionService final : public IConnectionService, public AdvancedService<IOUringTcpConnectionService> {
    public:
        IOUringTcpConnectionService(DependencyRegister &reg, Properties props);
        ~IOUringTcpConnectionService() final = default;

        tl::expected<uint64_t, SendErrorReason> sendAsync(std::vector<uint8_t>&& msg) final;
        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(ILogger &logger, IService &isvc) noexcept;
        void removeDependencyInstance(ILogger &logger, IService &isvc) noexcept;

        void addDependencyInstance(IIOUringQueue &, IService&) noexcept;
        void removeDependencyInstance(IIOUringQueue &, IService&) noexcept;

        std::function<void(int32_t)> createRecvHandler() noexcept;

        friend DependencyRegister;

        static uint64_t tcpConnId;
        int _socket;
        uint64_t _id;
        uint64_t _msgIdCounter{};
        int64_t _sendTimeout{250'000};
        int64_t _recvTimeout{250'000};
        bool _quit{};
        IIOUringQueue *_q{};
        ILogger *_logger{};
        std::vector<uint8_t> _recvBuf{};
        std::function<void(std::span<uint8_t>)> _recvHandler;
    };
}