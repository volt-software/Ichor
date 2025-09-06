#pragma once

#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32) && !defined(__APPLE__)) || defined(__CYGWIN__)

#include <ichor/services/network/IHostService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>

namespace Ichor::v1 {
    struct NewSocketEvent final : public Ichor::Event {
        NewSocketEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, int _socket) noexcept : Event(_id, _originatingService, _priority), socket(_socket) {}
        ~NewSocketEvent() final = default;

        [[nodiscard]] std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] NameHashType get_type() const noexcept final {
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

        void addDependencyInstance(ILogger &logger, IService &isvc);
        void removeDependencyInstance(ILogger &logger, IService &isvc);

        void addDependencyInstance(ITimerFactory &logger, IService &isvc);
        void removeDependencyInstance(ITimerFactory &logger, IService &isvc);

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
        ILogger *_logger{};
        ITimerFactory *_timerFactory{};
        tl::optional<TimerRef> _timer{};
        std::vector<ServiceIdType> _connections;
        EventHandlerRegistration _newSocketEventHandlerRegistration{};
    };
}

#endif
