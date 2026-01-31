#pragma once

#ifndef ICHOR_USE_LIBURING
#error "Ichor has not been compiled with io_uring support"
#endif

#include <ichor/services/network/IHostService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/event_queues/IIOUringQueue.h>
#include <ichor/ScopedServiceProxy.h>

namespace Ichor::v1 {
    /**
     * Service for creating a TCP host
     *
     * Properties:
     * - "Address" std::string - What address to bind to (default INADDR_ANY)
     * - "Port" uint16_t - What port to bind to (required)
     * - "Priority" uint64_t - Which priority to use for inserted events (default INTERNAL_EVENT_PRIORITY)
     * - "ListenBacklogSize" uint16_t - Maximum length of the queue of pending connections (default 100)
     * - "TimeoutSendUs" int64_t - Timeout in microseconds for send calls (default 250'000)
     * - "TimeoutRecvUs" int64_t - Timeout in microseconds for recv calls (default 250'000)
     * - "BufferEntries" uint32_t - BufferEntries config to pass on to newly created connections (default: none)
     * - "BufferEntrySize" uint32_t - BufferEntrySize config to pass on to newly created connections (default: none)
     */
    class IOUringTcpHostService final : public IHostService, public AdvancedService<IOUringTcpHostService> {
    public:
        IOUringTcpHostService(DependencyRegister &reg, Properties props);
        ~IOUringTcpHostService() final = default;

        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService &isvc) noexcept;
        void removeDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService &isvc) noexcept;

        void addDependencyInstance(Ichor::ScopedServiceProxy<IIOUringQueue*>, IService&) noexcept;
        void removeDependencyInstance(Ichor::ScopedServiceProxy<IIOUringQueue*>, IService&) noexcept;

        std::function<void(io_uring_cqe*)> createAcceptHandler() noexcept;

        friend DependencyRegister;
        friend DependencyManager;

        int _socket;
        int _bindFd;
        uint64_t _priority;
        uint16_t _listenBacklogSize{100};
        int64_t _sendTimeout{250'000};
        int64_t _recvTimeout{250'000};
		tl::optional<uint32_t> _bufferEntries{};
		tl::optional<uint32_t> _bufferEntrySize{};
        bool _quit;
        Ichor::ScopedServiceProxy<IIOUringQueue*> _q {};
        Ichor::ScopedServiceProxy<ILogger*> _logger {};
        std::vector<ServiceIdType> _connections;
        AsyncManualResetEvent _quitEvt;
    };
}
