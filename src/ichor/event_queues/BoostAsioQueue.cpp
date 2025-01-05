#ifndef ICHOR_USE_BOOST_BEAST
#error "Boost not enabled."
#endif

#include <boost/asio/steady_timer.hpp>
#include <ichor/event_queues/BoostAsioQueue.h>
#include <ichor/DependencyManager.h>
#include <ichor/dependency_management/InternalServiceLifecycleManager.h>

namespace Ichor::Detail {
    extern std::atomic<bool> sigintQuit;
    extern std::atomic<bool> registeredSignalHandler;
    void on_sigint([[maybe_unused]] int sig);
}

Ichor::BoostAsioQueue::BoostAsioQueue() : _context(BOOST_ASIO_CONCURRENCY_HINT_UNSAFE_IO) {
    _threadId = std::this_thread::get_id(); // re-set in functions below, because adding events when the queue isn't running yet cannot be done from another thread.
}

Ichor::BoostAsioQueue::BoostAsioQueue(uint64_t unused) : _context(BOOST_ASIO_CONCURRENCY_HINT_UNSAFE_IO) {
    _threadId = std::this_thread::get_id(); // re-set in functions below, because adding events when the queue isn't running yet cannot be done from another thread.
}

Ichor::BoostAsioQueue::~BoostAsioQueue() {
    if(_dm) {
        stopDm();
    }

    if(Detail::registeredSignalHandler) {
        if(::signal(SIGINT, SIG_DFL) == SIG_ERR) {
            //                fmt::println("Couldn't unset signal handler");
        }
    }
}

net::io_context& Ichor::BoostAsioQueue::getContext() noexcept {
    return _context;
}

bool Ichor::BoostAsioQueue::fibersShouldStop() const noexcept {
    return _quit.load(std::memory_order_acquire);
}

bool Ichor::BoostAsioQueue::empty() const {
    return !_dm || !_dm->isRunning();
}

uint64_t Ichor::BoostAsioQueue::size() const {
    return (_dm && _dm->isRunning()) ? 1 : 0;
}

bool Ichor::BoostAsioQueue::is_running() const noexcept {
    return !_quit.load(std::memory_order_acquire);
}

Ichor::NameHashType Ichor::BoostAsioQueue::get_queue_name_hash() const noexcept {
    return typeNameHash<BoostAsioQueue>();
}


void Ichor::BoostAsioQueue::pushEventInternal(uint64_t priority, std::unique_ptr<Event> &&event) {
    if(!event) [[unlikely]] {
        fmt::println("Pushing nullptr");
        std::terminate();
    }

    if(shouldQuit()) {
        //fmt::println("pushEventInternal, should quit! not inserting {}", event->get_name());
        return;
    }

    // TODO hardening
    //#ifdef ICHOR_USE_HARDENING
    //            if(originatingServiceId != 0 && _services.find(originatingServiceId) == _services.end()) [[unlikely]] {
    //                std::terminate();
    //            }
    //#endif

//    fmt::println("pushing {}:{}", event->id, event->get_name());
    net::post(_context, [this, event = std::move(event)]() mutable {
        if constexpr (DO_INTERNAL_DEBUG) {
            if(std::this_thread::get_id() != _threadId) {
                fmt::println("running on wrong thread");
                std::terminate();
            }
        }
        // fmt::println("processing {}:{}", event->id, event->get_name());
        processEvent(event);
        shouldAddQuitEvent();
    });
}

bool Ichor::BoostAsioQueue::shouldQuit() {
    if(_quitEventSent.load(std::memory_order_acquire) && std::chrono::steady_clock::now() - _whenQuitEventWasSent >= std::chrono::milliseconds(_quitTimeoutMs)) [[unlikely]] {
        _quit.store(true, std::memory_order_release);
    }

    return _quit.load(std::memory_order_acquire);
}

void Ichor::BoostAsioQueue::quit() {
    _quit.store(true, std::memory_order_release);
}

void Ichor::BoostAsioQueue::shouldAddQuitEvent() {
    bool const shouldQuit = Detail::sigintQuit.load(std::memory_order_acquire);

    if(shouldQuit && !_quitEventSent.load(std::memory_order_acquire)) {
        pushEventInternal(INTERNAL_EVENT_PRIORITY, std::make_unique<QuitEvent>(getNextEventId(), 0, INTERNAL_EVENT_PRIORITY));
        _quitEventSent.store(true, std::memory_order_release);
        _whenQuitEventWasSent = std::chrono::steady_clock::now();
    }
}

bool Ichor::BoostAsioQueue::start(bool captureSigInt) {
    if(!_dm) [[unlikely]] {
        fmt::println("Please create a manager first!");
        return false;
    }

    if(captureSigInt && !Ichor::Detail::registeredSignalHandler.exchange(true)) {
        if(::signal(SIGINT, Ichor::Detail::on_sigint) == SIG_ERR) {
            fmt::println("Couldn't set signal");
            return false;
        }
    }
    _threadId = std::this_thread::get_id();

    addInternalServiceManager(std::make_unique<Detail::InternalServiceLifecycleManager<IBoostAsioQueue>>(this));

    startDm();

    net::spawn(_context, [this](net::yield_context yield) {
        if constexpr (DO_INTERNAL_DEBUG) {
            if(std::this_thread::get_id() != _threadId) {
                fmt::println("running on wrong thread");
                std::terminate();
            }
        }
        net::steady_timer t{_context};
        while (!_quit) {
            t.expires_after(std::chrono::milliseconds(10));
            t.async_wait(yield);
        }
    }ASIO_SPAWN_COMPLETION_TOKEN);

    while (!_context.stopped()) {
        try {
            _context.run();
        } catch (boost::system::system_error const &e) {
            fmt::println("io context {} system error {}", _dm->getId(), e.what());
        } catch (std::exception const &e) {
            fmt::println("io context {} std error {}", _dm->getId(), e.what());
        }
    }

    return true;
}
