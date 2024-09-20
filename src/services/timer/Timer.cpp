#include <ichor/services/timer/Timer.h>
#include <ichor/events/RunFunctionEvent.h>
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
#include <windows.h>
#include <processthreadsapi.h>
#include <fmt/xchar.h>
#endif

using namespace std::chrono_literals;

Ichor::Timer::Timer(IEventQueue& queue, uint64_t timerId, uint64_t svcId) noexcept : _queue(queue), _timerId(timerId), _requestingServiceId(svcId) {
    fmt::println("Timer for {}", _requestingServiceId);
    stopTimer();
}

Ichor::Timer::~Timer() noexcept {
    stopTimer();
    if(_eventInsertionThread && _eventInsertionThread->joinable()) {
        _eventInsertionThread->join();
    }
}

void Ichor::Timer::startTimer() {
    startTimer(false);
}

void Ichor::Timer::startTimer(bool fireImmediately) {
    if(!_fn && !_fnAsync) {
        throw std::runtime_error("No callback set.");
    }

    bool expected = true;
    if(_quit.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
        if(_eventInsertionThread && _eventInsertionThread->joinable()) {
            _eventInsertionThread->join();
        }
        _eventInsertionThread = std::make_unique<std::thread>([this, fireImmediately]() { this->insertEventLoop(fireImmediately); });
#if defined(__linux__) || defined(__CYGWIN__)
        pthread_setname_np(_eventInsertionThread->native_handle(), fmt::format("Tmr#{}", _timerId).c_str());
#endif
    }
}

void Ichor::Timer::stopTimer() {
    bool expected = false;
    if(_quit.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        if(_eventInsertionThread->joinable()) {
            _eventInsertionThread->join();
        }
        _eventInsertionThread = nullptr;
    }
}

bool Ichor::Timer::running() const noexcept {
    return !_quit.load(std::memory_order_acquire);
};

void Ichor::Timer::setCallbackAsync(std::function<AsyncGenerator<IchorBehaviour>()> fn) {
    if(running()) {
        std::terminate();
    }

    _fnAsync = std::move(fn);
    _fn = {};
}

void Ichor::Timer::setCallback(std::function<void()> fn) {
    if(running()) {
        std::terminate();
    }

    _fnAsync = {};
    _fn = std::move(fn);
}

void Ichor::Timer::setInterval(uint64_t nanoseconds) noexcept {
    _intervalNanosec.store(nanoseconds, std::memory_order_release);
}


void Ichor::Timer::setPriority(uint64_t priority) noexcept {
    _priority.store(priority, std::memory_order_release);
}

uint64_t Ichor::Timer::getPriority() const noexcept {
    return _priority.load(std::memory_order_acquire);
}

void Ichor::Timer::setFireOnce(bool fireOnce) noexcept {
    _fireOnce.store(fireOnce, std::memory_order_release);
}

bool Ichor::Timer::getFireOnce() const noexcept {
    return _fireOnce.load(std::memory_order_acquire);
}

uint64_t Ichor::Timer::getTimerId() const noexcept {
    return _timerId;
}

void Ichor::Timer::insertEventLoop(bool fireImmediately) {
#if defined(__APPLE__)
    pthread_setname_np(fmt::format("Tmr#{}", _timerId).c_str());
#endif
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
    SetThreadDescription(GetCurrentThread(), fmt::format(L"Tmr#{}", _timerId).c_str());
#endif

    auto now = std::chrono::steady_clock::now();
    auto next = now;
    if(!fireImmediately) {
        next += std::chrono::nanoseconds(_intervalNanosec.load(std::memory_order_acquire));
    }
    while(!_quit.load(std::memory_order_acquire)) {
        while(now < next) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(_intervalNanosec.load(std::memory_order_acquire)/10));
            now = std::chrono::steady_clock::now();

            if(_quit.load(std::memory_order_acquire)) {
                return;
            }
        }
        // Make copy of function, in case setCallback() gets called during async stuff.
        if(_fnAsync) {
            _queue.pushPrioritisedEvent<RunFunctionEventAsync>(_requestingServiceId, getPriority(), _fnAsync);
        } else {
            _queue.pushPrioritisedEvent<RunFunctionEvent>(_requestingServiceId, getPriority(), _fn);
        }

        if(_fireOnce.load(std::memory_order_acquire)) {
            bool expected = false;
            _quit.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
            break;
        }

        next += std::chrono::nanoseconds(_intervalNanosec.load(std::memory_order_acquire));
    }
}
