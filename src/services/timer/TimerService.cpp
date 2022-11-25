#include <ichor/services/timer/TimerService.h>

Ichor::Timer::~Timer() noexcept {
    stopTimer();
}

Ichor::StartBehaviour Ichor::Timer::start() {
    return Ichor::StartBehaviour::SUCCEEDED;
}

Ichor::StartBehaviour Ichor::Timer::stop() {
    stopTimer();
    return Ichor::StartBehaviour::SUCCEEDED;
}

void Ichor::Timer::startTimer() {
    startTimer(false);
}

void Ichor::Timer::startTimer(bool fireImmediately) {
    if(!_fn) {
        throw std::runtime_error("No callback set.");
    }

    bool expected = true;
    if(_quit.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
        _eventInsertionThread = std::make_unique<std::thread>([this, fireImmediately]() { this->insertEventLoop(fireImmediately); });
#ifdef __linux__
        pthread_setname_np(_eventInsertionThread->native_handle(), fmt::format("Tmr #{}", getServiceId()).c_str());
#endif
    }
}

void Ichor::Timer::stopTimer() {
    bool expected = false;
    if(_quit.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        _eventInsertionThread->join();
        _eventInsertionThread = nullptr;
    }
}

bool Ichor::Timer::running() const noexcept {
    return !_quit.load(std::memory_order_acquire);
};

void Ichor::Timer::setCallback(IService *svc, decltype(RunFunctionEvent::fun) fn) {
    _requestingServiceId = svc->getServiceId();
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

void Ichor::Timer::insertEventLoop(bool fireImmediately) {
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
        getManager().pushPrioritisedEvent<RunFunctionEvent>(_requestingServiceId, getPriority(), _fn);

        next += std::chrono::nanoseconds(_intervalNanosec.load(std::memory_order_acquire));
    }
}