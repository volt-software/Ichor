#include <ichor/Assert.h>
#include <ichor/services/timer/Timer.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/ScopeGuard.h>
#include <fmt/format.h>
#include <mutex>
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
#include <windows.h>
#include <processthreadsapi.h>
#include <fmt/xchar.h>
#endif

using namespace std::chrono_literals;

Ichor::v1::Timer::Timer(Ichor::ScopedServiceProxy<IEventQueue*> queue, uint64_t timerId, ServiceIdType svcId) noexcept : _queue(queue), _timerId(timerId), _requestingServiceId(svcId) {
    INTERNAL_IO_DEBUG("Timer for {}", _requestingServiceId);
}

Ichor::v1::Timer::~Timer() noexcept {
    {
        std::unique_lock l{_m};
        if(_state == TimerState::RUNNING || _state == TimerState::STARTING) {
            _state = TimerState::STOPPING;
        }
        _wakeup.notify_all();
    }
    if(_eventInsertionThread && _eventInsertionThread->joinable()) {
        _eventInsertionThread->join();
    }
}

bool Ichor::v1::Timer::startTimer() {
    return startTimer(false);
}

bool Ichor::v1::Timer::startTimer(bool fireImmediately) {
    ICHOR_ASSERT_ALWAYS(_fn || _fnAsync, "timer {} for service {} started without a callback", _timerId, _requestingServiceId);
    std::unique_lock l{_m};
    INTERNAL_IO_DEBUG("timer {} for {} startTimer({}) {} {}", _timerId, _requestingServiceId, fireImmediately, _state, _quitCbs.size());
    if(_state == TimerState::STOPPED || _state == TimerState::STOPPING) {
        l.unlock();
        if(_eventInsertionThread && _eventInsertionThread->joinable()) {
            _eventInsertionThread->join();
        }
        l.lock();
        _state = TimerState::STARTING;
        _eventInsertionThread = std::make_unique<std::thread>([this, fireImmediately]() { this->insertEventLoop(fireImmediately); });
#if defined(__linux__) || defined(__CYGWIN__)
        pthread_setname_np(_eventInsertionThread->native_handle(), fmt::format("Tmr#{}", _timerId).c_str());
#endif
        _quitCbs.clear();
        return true;
    }
    return false;
}

bool Ichor::v1::Timer::stopTimer(std::function<void()> cb) {
    std::unique_lock l{_m};
    INTERNAL_IO_DEBUG("timer {} for {} stop {}", _timerId, _requestingServiceId, _state);
    // STARTING too: the thread hasn't started waiting yet, and quits when it sees the timer isn't STARTING anymore
    if(_state == TimerState::RUNNING || _state == TimerState::STARTING || _state == TimerState::STOPPING) {
        if(cb) {
            _quitCbs.emplace_back(std::move(cb));
        }
        _state = TimerState::STOPPING;
        _wakeup.notify_all();
        return true;
    } else if(cb) {
        if(_pendingQuitCbs) {
            std::unique_lock pl{_pendingQuitCbs->m};
            if(!_pendingQuitCbs->ran) {
                // The callbacks of the last stop didn't run yet. Run this one after them instead of before, e.g. a factory waiting for
                // its timers to be stopped could otherwise be destroyed before the earlier callbacks run.
                _pendingQuitCbs->cbs.emplace_back(std::move(cb));
                return false;
            }
        }
        // Not while holding the lock, the callback may use the timer (e.g. getState() or destroying it)
        l.unlock();
        cb();
    }
    return false;
}

Ichor::v1::TimerState Ichor::v1::Timer::getState() const noexcept {
    std::unique_lock l{_m};
    return _state;
};

void Ichor::v1::Timer::setCallbackAsync(std::function<AsyncGenerator<IchorBehaviour>()> fn) {
    std::unique_lock l{_m};
    ICHOR_ASSERT_ALWAYS(_state == TimerState::STOPPED, "setCallbackAsync() on timer {} for service {} while it is {}, the callback can only be changed while stopped", _timerId, _requestingServiceId, _state);

    _fnAsync = std::move(fn);
    _fn = {};
}

void Ichor::v1::Timer::setCallback(std::function<void()> fn) {
    std::unique_lock l{_m};
    ICHOR_ASSERT_ALWAYS(_state == TimerState::STOPPED, "setCallback() on timer {} for service {} while it is {}, the callback can only be changed while stopped", _timerId, _requestingServiceId, _state);

    _fnAsync = {};
    _fn = std::move(fn);
}

void Ichor::v1::Timer::setInterval(uint64_t nanoseconds) noexcept {
    std::unique_lock l{_m};
    _intervalNanosec = nanoseconds;
    // the next expiry is relative to the previous one, using the new interval
    _wakeup.notify_all();
}


void Ichor::v1::Timer::setPriority(uint64_t priority) noexcept {
    std::unique_lock l{_m};
    _priority = priority;
}

uint64_t Ichor::v1::Timer::getPriority() const noexcept {
    std::unique_lock l{_m};
    return _priority;
}

void Ichor::v1::Timer::setFireOnce(bool fireOnce) noexcept {
    std::unique_lock l{_m};
    _fireOnce = fireOnce;
}

bool Ichor::v1::Timer::getFireOnce() const noexcept {
    std::unique_lock l{_m};
    return _fireOnce;
}

uint64_t Ichor::v1::Timer::getTimerId() const noexcept {
    return _timerId;
}

Ichor::ServiceIdType Ichor::v1::Timer::getRequestingServiceId() const noexcept {
    return _requestingServiceId;
}

void Ichor::v1::Timer::insertEventLoop(bool fireImmediately) {
    INTERNAL_IO_DEBUG("timer {} for {} insertEventLoop", _timerId, _requestingServiceId);
#if defined(__APPLE__)
    pthread_setname_np(fmt::format("Tmr#{}", _timerId).c_str());
#endif
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
    SetThreadDescription(GetCurrentThread(), fmt::format(L"Tmr#{}", _timerId).c_str());
#endif

    ScopeGuard sg{[this]() {
        std::unique_lock l{_m};
        auto pending = std::make_shared<Detail::TimerQuitCallbacks>();
        pending->cbs = std::move(_quitCbs);
        _quitCbs.clear();
        _pendingQuitCbs = pending;
        _queue->pushPrioritisedEvent<RunFunctionEvent>(ServiceIdType{0}, _priority, [pending = std::move(pending)](){
            std::vector<std::function<void()>> quitCbs;
            {
                std::unique_lock pl{pending->m};
                pending->ran = true;
                quitCbs = std::move(pending->cbs);
            }
            for(auto &cb : quitCbs) {
                cb();
            }
        });
        _state = TimerState::STOPPED;
    }};
    std::unique_lock l{_m};
    if(_state != TimerState::STARTING) {
        INTERNAL_IO_DEBUG("timer {} for {} insertEventLoop quit before start??", _timerId, _requestingServiceId);
        return;
    }
    _state = TimerState::RUNNING;
    // Expiries are relative to the previous one instead of to when the callback got pushed, so that the interval doesn't drift.
    auto previous = std::chrono::steady_clock::now();
    bool fireNow = fireImmediately;
    while(_state == TimerState::RUNNING) {
        if(!fireNow) {
            // Woken up early by stopTimer(), setInterval() or the destructor
            while(_state == TimerState::RUNNING) {
                auto const interval = std::chrono::nanoseconds(_intervalNanosec);
                auto const next = previous + interval;
                auto const now = std::chrono::steady_clock::now();
                if(now >= next) {
                    // If we're behind by more than an interval (e.g. the interval got shortened), skip the missed expiries instead of firing them all at once.
                    previous = now - next >= interval ? now : next;
                    break;
                }
                INTERNAL_IO_DEBUG("timer {} for {} waiting for {}", _timerId, _requestingServiceId, (next - now).count());
                _wakeup.wait_until(l, next);
            }

            if(_state != TimerState::RUNNING) {
                INTERNAL_IO_DEBUG("timer {} for {} stopping because state is {}", _timerId, _requestingServiceId, _state);
                return;
            }
        }
        fireNow = false;

        INTERNAL_IO_DEBUG("timer {} for {} pushing event", _timerId, _requestingServiceId);
        // Make copy of function, in case setCallback() gets called during async stuff.
        if(_fnAsync) {
            _queue->pushPrioritisedEvent<RunFunctionEventAsync>(_requestingServiceId, _priority, _fnAsync);
        } else {
            _queue->pushPrioritisedEvent<RunFunctionEvent>(_requestingServiceId, _priority, _fn);
        }

        if(_fireOnce) {
            INTERNAL_IO_DEBUG("timer {} for {} stopping, fireonce", _timerId, _requestingServiceId);
            _state = TimerState::STOPPING;
            break;
        }
    }
}
