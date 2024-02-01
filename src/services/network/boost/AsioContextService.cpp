#include <ichor/DependencyManager.h>
#include <ichor/services/network/boost/AsioContextService.h>
#include <ichor/events/RunFunctionEvent.h>
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
#include <processthreadsapi.h>
#include <fmt/xchar.h>
#endif

Ichor::AsioContextService::AsioContextService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
    if(getProperties().contains("Threads")) {
        _threads = Ichor::any_cast<uint64_t>(getProperties()["Threads"]);
        if(_threads == 0) {
            _threads = std::thread::hardware_concurrency();
        }
    }

    reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
}

Ichor::AsioContextService::~AsioContextService() {
    for(auto &thread : _asioThreads) {
        if(thread.joinable()) {
            // "We're in a bad situation, sir, and do not know how to recover."
            std::terminate();
        }
    }
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::AsioContextService::start() {
    _quit = false;
    if(_threads == 1) {
        _context = std::make_unique<net::io_context>(BOOST_ASIO_CONCURRENCY_HINT_UNSAFE_IO);
    } else {
        _context = std::make_unique<net::io_context>();
    }

    ICHOR_LOG_INFO(_logger, "Using boost version {} beast version {}", BOOST_VERSION, BOOST_BEAST_VERSION);

    net::spawn(*_context, [this, &queue = GetThreadLocalEventQueue()](net::yield_context yield) {
        // notify start()
        queue.pushPrioritisedEvent<RunFunctionEvent>(getServiceId(), INTERNAL_COROUTINE_EVENT_PRIORITY, [this]() {
            _startStopEvent.set();
        });

        net::steady_timer t{*_context};
        while (!_quit) {
            t.expires_after(std::chrono::milliseconds(10));
            t.async_wait(yield);
        }
        INTERNAL_DEBUG("+++++++++++++++++++++++++++++++++++++++++++++++ NOTIFY STOP ++++++++++++++++++++++++++++++++");

        // notify stop()
        queue.pushPrioritisedEvent<RunFunctionEvent>(getServiceId(), INTERNAL_COROUTINE_EVENT_PRIORITY, [this]() {
            _startStopEvent.set();
        });
    }ASIO_SPAWN_COMPLETION_TOKEN);

    for(uint64_t i = 0; i < _threads; i++) {
        [[maybe_unused]] auto &thread = _asioThreads.emplace_back([this, i]() {
#if defined(__APPLE__)
            pthread_setname_np(fmt::format("Asio#{}-{}", getServiceId(), i).c_str());
#endif
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
            SetThreadDescription(GetCurrentThread(), fmt::format(L"Asio#{}-{}", getServiceId(), i).c_str());
#endif
            INTERNAL_DEBUG("AsioContext started");
            while (!_context->stopped()) {
                try {
                    _context->run();
                } catch (boost::system::system_error const &e) {
                    ICHOR_LOG_ERROR(_logger, "io context {}-{} system error {}", getServiceId(), i, e.what());
                } catch (std::exception const &e) {
                    ICHOR_LOG_ERROR(_logger, "io context {}-{} std error {}", getServiceId(), i, e.what());
                }
            }

            INTERNAL_DEBUG("AsioContext stopped");
        });

#if defined(__linux__) || defined(__CYGWIN__)
        pthread_setname_np(thread.native_handle(), fmt::format("Asio#{}-{}", getServiceId(), i).c_str());
#endif
    }

    co_await _startStopEvent;

    _startStopEvent.reset();

    co_return {};
}

Ichor::Task<void> Ichor::AsioContextService::stop() {
    _quit.store(true, std::memory_order_release);
    INTERNAL_DEBUG("+++++++++++++++++++++++++++++++++++++++++++++++ STOP ++++++++++++++++++++++++++++++++");

    if(!_context->stopped()) {
        INTERNAL_DEBUG("+++++++++++++++++++++++++++++++++++++++++++++++ pre wait\n");
        co_await _startStopEvent;
        INTERNAL_DEBUG("+++++++++++++++++++++++++++++++++++++++++++++++ post wait\n");

        _context->stop();
    }

    INTERNAL_DEBUG("+++++++++++++++++++++++++++++++++++++++++++++++ pre join\n");
    for(auto &thread : _asioThreads) {
        thread.join();
    }

    INTERNAL_DEBUG("+++++++++++++++++++++++++++++++++++++++++++++++ post join\n");
    _context = nullptr;
    _asioThreads.clear();

    INTERNAL_DEBUG("+++++++++++++++++++++++++++++++++++++++++++++++ post clear\n");
    co_return;
}

void Ichor::AsioContextService::addDependencyInstance(ILogger &logger, IService &) {
    _logger = &logger;
}

void Ichor::AsioContextService::removeDependencyInstance(ILogger &logger, IService&) {
    _logger = nullptr;
}

net::io_context* Ichor::AsioContextService::getContext() noexcept {
    return _context.get();
}

bool Ichor::AsioContextService::fibersShouldStop() const noexcept {
    return _quit.load(std::memory_order_acquire);
}

uint64_t Ichor::AsioContextService::threadCount() const noexcept {
    return _threads;
}
