#pragma once

#include <ichor/event_queues/IBoostAsioQueue.h>
#include <ichor/services/network/IConnectionService.h>
#include <ichor/services/network/IHostService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <boost/beast.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/circular_buffer.hpp>
#include <ichor/ServiceExecutionScope.h>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace Ichor::Boost::v1 {
    namespace Detail {
        struct WsConnectionOutboxMessage final {
            std::vector<uint8_t> msg;
            AsyncManualResetEvent *evt;
            bool *success;
        };
    }

    template <typename InterfaceT> requires DerivedAny<InterfaceT, Ichor::v1::IConnectionService, Ichor::v1::IHostConnectionService, Ichor::v1::IClientConnectionService>
    class WsConnectionService final : public InterfaceT, public AdvancedService<WsConnectionService<InterfaceT>> {
    public:
        WsConnectionService(DependencyRegister &reg, Properties props);
        ~WsConnectionService() final = default;

        Task<tl::expected<void, Ichor::v1::IOError>> sendAsync(std::vector<uint8_t>&& msg) final;
        Task<tl::expected<void, Ichor::v1::IOError>> sendAsync(std::vector<std::vector<uint8_t>>&& msgs) final;
        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

		[[nodiscard]] bool isClient() const noexcept final;
		void setReceiveHandler(std::function<void(std::span<uint8_t const>)>) final;

    private:
        Task<tl::expected<void, StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(Ichor::ScopedServiceProxy<Ichor::v1::ILogger*> logger, IService &isvc);
        void removeDependencyInstance(Ichor::ScopedServiceProxy<Ichor::v1::ILogger*> logger, IService &isvc);

        void addDependencyInstance(Ichor::ScopedServiceProxy<Ichor::v1::IHostService*>, IService &isvc);
        void removeDependencyInstance(Ichor::ScopedServiceProxy<Ichor::v1::IHostService*>, IService &isvc);

        void addDependencyInstance(Ichor::ScopedServiceProxy<IBoostAsioQueue*> q, IService&);
        void removeDependencyInstance(Ichor::ScopedServiceProxy<IBoostAsioQueue*> q, IService&);

        friend DependencyRegister;
        friend DependencyManager;

        void fail(beast::error_code, char const* what);
        void accept(net::yield_context yield); // for when a new connection from WsHost is established
        void connect(net::yield_context yield); // for when connecting as a client
        void read(net::yield_context &yield);

        std::shared_ptr<websocket::stream<beast::tcp_stream>> _ws{};
        uint64_t _priority{};
        bool _connected{};
        bool _quit{};
        Ichor::ScopedServiceProxy<Ichor::v1::ILogger*> _logger {};
        Ichor::ScopedServiceProxy<IBoostAsioQueue*> _queue {};
        std::unique_ptr<net::strand<net::io_context::executor_type>> _strand{};
        std::atomic<int64_t> _finishedListenAndRead{};
        AsyncManualResetEvent _startStopEvent{};
        boost::circular_buffer<Detail::WsConnectionOutboxMessage> _outbox{10};
		std::vector<std::vector<uint8_t>> _queuedMessages{};
		std::function<void(std::span<uint8_t const>)> _recvHandler;
    };
}
