#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/network_bundle/http/HttpContextService.h>


Ichor::HttpContextService::HttpContextService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
    reg.registerDependency<ILogger>(this, true);
}

Ichor::HttpContextService::~HttpContextService() {
    if (_httpThread.joinable()) {
        _quit = true;
        _httpContext->stop();
        _httpThread.join();
    }
}

Ichor::StartBehaviour Ichor::HttpContextService::start() {
    if(!_starting && _stopped) {
        _starting = true;
        _stopped = true;
        _quit = false;
        _httpContext = std::make_unique<net::io_context>();

        net::spawn(*_httpContext.get(), [this](net::yield_context yield) {
            _stopped = false;
            _starting = false;
            ICHOR_LOG_INFO(_logger, "HttpContext keep alive fiber started");
            while (!_quit) {
                net::steady_timer t{*_httpContext.get()};
                t.expires_after(std::chrono::milliseconds(50));
                t.async_wait(yield);
            }
            ICHOR_LOG_INFO(_logger, "HttpContext keep alive fiber stopped");
        });

        _httpThread = std::thread([this]() {
            ICHOR_LOG_INFO(_logger, "HttpContext started");
            boost::system::error_code ec;
            while (!ec && !_httpContext->stopped()) {
                _httpContext->run(ec);
                if (ec) {
                    ICHOR_LOG_ERROR(_logger, "ec error {}", ec.message());
                }
            }
            _starting = false;
            _stopped = true;
            ICHOR_LOG_INFO(_logger, "HttpContext stopped");
        });

#ifdef __linux__
        pthread_setname_np(_httpThread.native_handle(), fmt::format("HttpCon #{}", getServiceId()).c_str());
#endif
    }

    return _stopped || _starting ? Ichor::StartBehaviour::FAILED_AND_RETRY : Ichor::StartBehaviour::SUCCEEDED;
}

Ichor::StartBehaviour Ichor::HttpContextService::stop() {
    INTERNAL_DEBUG("HttpContextService stop: {} {}", _quit, _stopped);
    _quit = true;
    if (_stopped) {
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