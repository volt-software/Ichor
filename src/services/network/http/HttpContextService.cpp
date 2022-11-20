#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/DependencyManager.h>
#include <ichor/services/network/http/HttpContextService.h>


Ichor::HttpContextService::HttpContextService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
    reg.registerDependency<ILogger>(this, true);
}

Ichor::HttpContextService::~HttpContextService() {
    if (_httpThread.joinable()) {
        // "We're in a bad situation, sir, and do not know how to recover."
        std::terminate();
    }
}

Ichor::AsyncGenerator<void> Ichor::HttpContextService::start() {
    _quit = false;
    _httpContext = std::make_unique<net::io_context>(BOOST_ASIO_CONCURRENCY_HINT_UNSAFE_IO);

    net::spawn(*_httpContext, [this](net::yield_context yield) {
        // notify start()
        getManager().pushPrioritisedEvent<RunFunctionEvent>(getServiceId(), INTERNAL_COROUTINE_EVENT_PRIORITY, [this](DependencyManager &dm) -> AsyncGenerator<IchorBehaviour> {
            _startStopEvent.set();
            co_return {};
        });

        net::steady_timer t{*_httpContext};
        while (!_quit) {
            t.expires_after(std::chrono::milliseconds(50));
            t.async_wait(yield);
        }
        INTERNAL_DEBUG("+++++++++++++++++++++++++++++++++++++++++++++++ NOTIFY STOP ++++++++++++++++++++++++++++++++");

        // notify stop()
        getManager().pushPrioritisedEvent<RunFunctionEvent>(getServiceId(), INTERNAL_COROUTINE_EVENT_PRIORITY, [this](DependencyManager &dm) -> AsyncGenerator<IchorBehaviour> {
            _startStopEvent.set();
            co_return {};
        });
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

        INTERNAL_DEBUG("HttpContext stopped");
    });

#ifdef __linux__
    pthread_setname_np(_httpThread.native_handle(), fmt::format("HttpCon #{}", getServiceId()).c_str());
#endif

    co_await _startStopEvent;

    _startStopEvent.reset();

    co_return;
}

Ichor::AsyncGenerator<void> Ichor::HttpContextService::stop() {
    _quit = true;
    INTERNAL_DEBUG("+++++++++++++++++++++++++++++++++++++++++++++++ STOP ++++++++++++++++++++++++++++++++");

    if(!_httpContext->stopped()) {
        INTERNAL_DEBUG("+++++++++++++++++++++++++++++++++++++++++++++++ pre wait\n");
        co_await _startStopEvent;
        INTERNAL_DEBUG("+++++++++++++++++++++++++++++++++++++++++++++++ post wait\n");

        _httpContext->stop();
        while(_httpContext->poll_one() > 0);
    }

    INTERNAL_DEBUG("+++++++++++++++++++++++++++++++++++++++++++++++ pre join\n");
    _httpThread.join();
    _httpContext = nullptr;

    INTERNAL_DEBUG("+++++++++++++++++++++++++++++++++++++++++++++++ post join\n");
    co_return;
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