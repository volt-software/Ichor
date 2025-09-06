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

Ichor::v1::Timer::Timer(IEventQueue& queue, uint64_t timerId, uint64_t svcId) noexcept : _queue(&queue), _timerId(timerId), _requestingServiceId(svcId) {
    INTERNAL_IO_DEBUG("Timer for {}", _requestingServiceId);
}

Ichor::v1::Timer::~Timer() noexcept {
    {
        std::unique_lock l{_m};
        if(_state == TimerState::RUNNING) {
            _state = TimerState::STOPPING;
        }
    }
    if(_eventInsertionThread && _eventInsertionThread->joinable()) {
        _eventInsertionThread->join();
    }
}

// Ichor::v1::Timer& Ichor::v1::Timer::operator=(Ichor::v1::Timer &&o) noexcept {
//     _queue = o._queue;
//     _timerId = o._timerId;
//     _state = o._state;
//     _fireOnce = o._fireOnce;
//     _intervalNanosec = o._intervalNanosec;
//     _eventInsertionThread = std::move(o._eventInsertionThread);
//     _fnAsync = std::move(o._fnAsync);
//     _fn = std::move(o._fn);
//     _priority = o._priority;
//     _requestingServiceId = o._requestingServiceId;
//     _quitCbs = std::move(o._quitCbs);
//     _m = std::move(o._m);
//     return *this;
// }

bool Ichor::v1::Timer::startTimer() {
    return startTimer(false);
}

bool Ichor::v1::Timer::startTimer(bool fireImmediately) {
    if(!_fn && !_fnAsync) [[unlikely]] {
        fmt::println("No callback set.");
        std::terminate();
    }
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

bool Ichor::v1::Timer::stopTimer(std::function<void(void)> cb) {
    std::unique_lock l{_m};
    INTERNAL_IO_DEBUG("timer {} for {} stop {}", _timerId, _requestingServiceId, _state);
    if(_state == TimerState::RUNNING || _state == TimerState::STOPPING) {
        if(cb) {
            _quitCbs.emplace_back(std::move(cb));
        }
        _state = TimerState::STOPPING;
        return true;
    } else if(cb) {
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
    if(_state != TimerState::STOPPED) {
        std::terminate();
    }

    _fnAsync = std::move(fn);
    _fn = {};
}

void Ichor::v1::Timer::setCallback(std::function<void()> fn) {
    std::unique_lock l{_m};
    if(_state != TimerState::STOPPED) {
        std::terminate();
    }

    _fnAsync = {};
    _fn = std::move(fn);
}

void Ichor::v1::Timer::setInterval(uint64_t nanoseconds) noexcept {
    std::unique_lock l{_m};
    _intervalNanosec = nanoseconds;
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
        _queue->pushPrioritisedEvent<RunFunctionEvent>(0, _priority, [quitCbs = std::move(_quitCbs)](){
            for(auto &cb : quitCbs) {
                cb();
            }
        });
        _state = TimerState::STOPPED;
        _quitCbs.clear();
    }};
    auto now = std::chrono::steady_clock::now();
    auto next = now;
    std::unique_lock l{_m};
    if(!fireImmediately) {
        next += std::chrono::nanoseconds(_intervalNanosec);
    }
    if(_state != TimerState::STARTING) {
        INTERNAL_IO_DEBUG("timer {} for {} insertEventLoop quit before start??", _timerId, _requestingServiceId);
        return;
    }
    _state = TimerState::RUNNING;
    while(_state == TimerState::RUNNING) {
        while(now < next) {
            auto ns = _intervalNanosec/10;
            l.unlock();
            INTERNAL_IO_DEBUG("timer {} for {} sleeping for {}", _timerId, _requestingServiceId, ns);
            std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
            now = std::chrono::steady_clock::now();
            l.lock();

            if(_state != TimerState::RUNNING) {
                INTERNAL_IO_DEBUG("timer {} for {} stopping because state is {}", _timerId, _requestingServiceId, _state);
                return;
            }
        }
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

        next += std::chrono::nanoseconds(_intervalNanosec);
    }
}
