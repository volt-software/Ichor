#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/DependencyManager.h>
#include <ichor/services/network/http/HttpContextService.h>


Ichor::HttpContextService::HttpContextService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
    reg.registerDependency<ILogger>(this, true);
}

Ichor::HttpContextService::~HttpContextService() {
    if (_httpThread.joinable()) {
        _quit = true;
        while(!_keepAliveStopped) {
            std::this_thread::sleep_for(5ms);
        }
        _httpContext->stop();
        while(_httpContext->poll_one() > 0);
        _httpThread.join();
        _httpContext = nullptr;
    }
}

Ichor::StartBehaviour Ichor::HttpContextService::start() {
    if(!_starting && _stopped) {
        _starting = true;
        _stopped = true;
        _quit = false;
        _httpContext = std::make_unique<net::io_context>(BOOST_ASIO_CONCURRENCY_HINT_UNSAFE_IO);

        net::spawn(*_httpContext, [this](net::yield_context yield) {
            _stopped = false;
            _starting = false;
            INTERNAL_DEBUG("HttpContext keep alive fiber started");
            net::steady_timer t{*_httpContext};
            while (!_quit) {
                t.expires_after(std::chrono::milliseconds(50));
                t.async_wait(yield);
            }
            INTERNAL_DEBUG("HttpContext keep alive fiber stopped");
            _keepAliveStopped = true;
        });

        _httpThread = std::thread([this]() {
            INTERNAL_DEBUG("HttpContext started");
            while (!_httpContext->stopped()) {
                try {
                    _httpContext->run();
                } catch (boost::system::system_error const &e) {
                    ICHOR_LOG_ERROR(_logger, "http io context system error {}", e.what());
                } catch (std::exception const &e) {
                    ICHOR_LOG_ERROR(_logger, "http io context std error {}", e.what());
                }
            }
            _starting = false;
            _stopped = true;
            INTERNAL_DEBUG("HttpContext stopped");
        });

#ifdef __linux__
        pthread_setname_np(_httpThread.native_handle(), fmt::format("HttpCon #{}", getServiceId()).c_str());
#endif
    }

    // It can take a while to start the boost I/O context and thread
    // Prevent an event storm by delaying the event queue by sleeping
    std::this_thread::sleep_for(1ms);

    return _stopped || _starting ? Ichor::StartBehaviour::FAILED_AND_RETRY : Ichor::StartBehaviour::SUCCEEDED;
}

Ichor::StartBehaviour Ichor::HttpContextService::stop() {
    _quit = true;
    if (_stopped) {
        while(!_keepAliveStopped) {
            std::this_thread::sleep_for(5ms);
        }
        _httpContext->stop();
        while(_httpContext->poll_one() > 0);
        _httpThread.join();
        _httpContext = nullptr;
    }

    return _httpThread.joinable() ? Ichor::StartBehaviour::FAILED_AND_RETRY : Ichor::StartBehaviour::SUCCEEDED;
}

void Ichor::HttpContextService::addDependencyInstance(ILogger *logger, IService *) {
    _logger = logger;
}

void Ichor::HttpContextService::removeDependencyInstance(ILogger *logger, IService *) {
    _logger = nullptr;
}

net::io_context* Ichor::HttpContextService::getContext() noexcept {
    return _httpContext.get();
}

bool Ichor::HttpContextService::fibersShouldStop() noexcept {
    return _quit.load(std::memory_order_acquire);
}

#endif