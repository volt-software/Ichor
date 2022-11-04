#pragma once

#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/services/network/http/IHttpService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/TimerService.h>
#include <boost/beast.hpp>
#include <boost/asio/spawn.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace Ichor {
    class IHttpContextService {
    public:
        virtual net::io_context* getContext() noexcept = 0;
        virtual bool fibersShouldStop() noexcept = 0;

    protected:
        ~IHttpContextService() = default;
    };

    class HttpContextService final : public IHttpContextService, public Service<HttpContextService> {
    public:
        HttpContextService(DependencyRegister &reg, Properties props, DependencyManager *mng);
        ~HttpContextService() final;

        net::io_context* getContext() noexcept final;
        bool fibersShouldStop() noexcept final;

    private:
        StartBehaviour start() final;
        StartBehaviour stop() final;

        void addDependencyInstance(ILogger *logger, IService *isvc);
        void removeDependencyInstance(ILogger *logger, IService *isvc);

        friend DependencyRegister;

        std::unique_ptr<net::io_context> _httpContext{};
        std::thread _httpThread{};
        std::atomic<bool> _starting{};
        std::atomic<bool> _stopped{true};
        std::atomic<bool> _keepAliveStopped{};
        std::atomic<bool> _quit{};
        ILogger *_logger{nullptr};
    };
}

#endif