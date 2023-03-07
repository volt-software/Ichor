#pragma once

#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/services/network/http/IHttpService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/TimerService.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <boost/beast.hpp>
#include <boost/asio/spawn.hpp>

#if BOOST_VERSION >= 108000
#define ASIO_SPAWN_COMPLETION_TOKEN , [](std::exception_ptr e) { if (e) std::rethrow_exception(e); }
#else
#define ASIO_SPAWN_COMPLETION_TOKEN
#endif
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace Ichor {
    class IAsioContextService {
    public:
        virtual net::io_context* getContext() noexcept = 0;
        virtual bool fibersShouldStop() const noexcept = 0;
        virtual uint64_t threadCount() const noexcept = 0;

    protected:
        ~IAsioContextService() = default;
    };

    class AsioContextService final : public IAsioContextService, public AdvancedService<AsioContextService> {
    public:
        AsioContextService(DependencyRegister &reg, Properties props);
        ~AsioContextService() final;

        net::io_context* getContext() noexcept final;
        bool fibersShouldStop() const noexcept final;
        uint64_t threadCount() const noexcept final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(ILogger &logger, IService &isvc);
        void removeDependencyInstance(ILogger &logger, IService &isvc);

        friend DependencyRegister;

        std::unique_ptr<net::io_context> _context{};
        std::vector<std::thread> _asioThreads{};
        std::atomic<bool> _quit{};
        uint64_t _threads{1};
        ILogger *_logger{nullptr};
        AsyncManualResetEvent _startStopEvent{};
    };
}

#endif
