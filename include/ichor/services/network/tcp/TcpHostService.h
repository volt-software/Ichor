#pragma once

#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32) && !defined(__APPLE__)) || defined(__CYGWIN__)

#include <ichor/services/network/IHostService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/ServiceExecutionScope.h>

namespace Ichor::v1 {
    struct NewSocketEvent final : public Ichor::Event {
        constexpr NewSocketEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority, int _socket) noexcept : Event(_id, _originatingService, _priority), socket(_socket) {}
        constexpr ~NewSocketEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
            return TYPE;
        }

        int socket;
        static constexpr NameHashType TYPE = Ichor::typeNameHash<NewSocketEvent>();
        static constexpr std::string_view NAME = Ichor::typeName<NewSocketEvent>();
    };

    /**
     * Service for creating a TCP host
     *
     * Properties:
     * - "Address" std::string - What address to bind to (default INADDR_ANY)
     * - "Port" uint16_t - What port to bind to (required)
     * - "Priority" uint64_t - Which priority to use for inserted events (default INTERNAL_EVENT_PRIORITY)
     * - "TimeoutSendUs" int64_t - Timeout in microseconds for send calls (default 250'000)
     */
    class TcpHostService final : public IHostService, public AdvancedService<TcpHostService> {
    public:
        TcpHostService(DependencyRegister &reg, Properties props);
        ~TcpHostService() final = default;

        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService &isvc);
        void removeDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService &isvc);

        void addDependencyInstance(Ichor::ScopedServiceProxy<ITimerFactory*> logger, IService &isvc);
        void removeDependencyInstance(Ichor::ScopedServiceProxy<ITimerFactory*> logger, IService &isvc);

        AsyncGenerator<IchorBehaviour> handleEvent(NewSocketEvent const &evt);
        void acceptHandler();

        friend DependencyRegister;
        friend DependencyManager;

        int _socket;
        int _bindFd;
        uint64_t _priority;
        int64_t _sendTimeout{250'000};
        int64_t _recvTimeout{250'000};
        bool _quit;
        Ichor::ScopedServiceProxy<ILogger*> _logger {};
        Ichor::ScopedServiceProxy<ITimerFactory*> _timerFactory {};
        tl::optional<TimerRef> _timer{};
        std::vector<ServiceIdType> _connections;
        EventHandlerRegistration _newSocketEventHandlerRegistration{};
    };
}

#endif
