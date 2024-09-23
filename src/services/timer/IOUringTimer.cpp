#include <ichor/services/timer/IOUringTimer.h>
#include <ichor/events/RunFunctionEvent.h>


using namespace std::chrono_literals;

Ichor::IOUringTimer::IOUringTimer(IIOUringQueue& queue, uint64_t timerId, uint64_t svcId) noexcept : _q(queue), _timerId(timerId), _requestingServiceId(svcId) {
    fmt::println("[{:L}] IOUringTimer for {}", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), _requestingServiceId);
    stopTimer();

    if constexpr(DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (_timerId > std::numeric_limits<unsigned int>::max()) {
            fmt::println("timerId overflow for io_uring");
            std::terminate();
        }
    }
}

void Ichor::IOUringTimer::startTimer() {
    startTimer(false);
}

void Ichor::IOUringTimer::startTimer(bool fireImmediately) {
    if(!_fn && !_fnAsync) [[unlikely]] {
        throw std::runtime_error("No callback set.");
    }

    if(_quit) {
        fmt::println("[{:L}] Starting IOUringTimer {} for {} with {} s {} ns timeout", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), _timerId, _requestingServiceId, _timespec.tv_sec, _timespec.tv_nsec);
        // TODO instead of relying on UringResponseEvent, create a decltype(_fnAsync) event directly.
        auto *sqe = _q.getSqeWithData(_requestingServiceId, createNewHandler());
        io_uring_prep_timeout(sqe, &_timespec, 0, 0);
        _uringTimerId = sqe->user_data;

        _quit = false;
    }
}

void Ichor::IOUringTimer::stopTimer(/*std::function<void()> stopCb*/) {
    if(!_quit) {
        fmt::println("Stopping timer {}", _timerId);
//        _stopCb = std::move(stopCb);
        auto *sqe = _q.getSqeWithData(_requestingServiceId, [timerId = _timerId](io_uring_cqe *cqe) {
            if(cqe->res < 0) {
                fmt::println("[{:L}] Couldn't stop timer {} because {}", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), timerId, cqe->res);
            } else {
                fmt::println("[{:L}] Stopped timer {} because {}", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), timerId, cqe->res);
            }
        });
        io_uring_prep_timeout_remove(sqe, _uringTimerId, 0);
    } /*else {
        _quit = true;
        if(stopCb) {
            stopCb();
        }
    }*/
}

bool Ichor::IOUringTimer::running() const noexcept {
    return !_quit;
};

void Ichor::IOUringTimer::setCallbackAsync(std::function<AsyncGenerator<IchorBehaviour>()> fn) {
    if(running()) {
        std::terminate();
    }

    _fnAsync = std::move(fn);
    _fn = {};
}

void Ichor::IOUringTimer::setCallback(std::function<void()> fn) {
    if(running()) {
        std::terminate();
    }

    _fnAsync = {};
    _fn = std::move(fn);
}

void Ichor::IOUringTimer::setInterval(uint64_t nanoseconds) noexcept {
    if constexpr(DO_INTERNAL_DEBUG || DO_HARDENING) {
        if(nanoseconds > std::numeric_limits<decltype(_timespec.tv_nsec)>::max()) {
            fmt::println("given nanoseconds {} is greater than allowed by type {}", nanoseconds, std::numeric_limits<decltype(_timespec.tv_nsec)>::max());
            std::terminate();
        }
    }

    _timespec.tv_nsec = static_cast<decltype(_timespec.tv_nsec)>(nanoseconds);

    if(!_quit) {
        auto *sqe = _q.getSqeWithData(_requestingServiceId, [timerId = _timerId](io_uring_cqe *cqe) {
            if (cqe->res < 0) {
                fmt::println("[{:L}] Couldn't stop timer {} because {}", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), timerId, cqe->res);
            }
        });
        io_uring_prep_timeout_update(sqe, &_timespec, _uringTimerId, 0);
    }
}


void Ichor::IOUringTimer::setPriority(uint64_t priority) noexcept {
    _priority = priority;
}

uint64_t Ichor::IOUringTimer::getPriority() const noexcept {
    return _priority;
}

void Ichor::IOUringTimer::setFireOnce(bool fireOnce) noexcept {
    _fireOnce = fireOnce;
}

bool Ichor::IOUringTimer::getFireOnce() const noexcept {
    return _fireOnce;
}

uint64_t Ichor::IOUringTimer::getTimerId() const noexcept {
    return _timerId;
}

std::function<void(io_uring_cqe *)> Ichor::IOUringTimer::createNewHandler() noexcept {
    fmt::println("[{:L}] IOUringTimer {} createNewHandler", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), _timerId);
    return [this, timerId = _timerId](io_uring_cqe *cqe) {
        if(cqe->res <= 0 && cqe->res != -ETIME) {
            if(cqe->res != -ECANCELED) {
                fmt::println("[{:L}] IOUringTimer {} error {}", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), timerId, cqe->res);
            }
            return;
        }
        fmt::println("[{:L}] IOUringTimer {} error {}", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), timerId, cqe->res);

        fmt::println("[{:L}] IOUringTimer {} handler quit {}", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), timerId, _quit);

//        if(_stopCb) {
//            _quit = true;
//            _stopCb();
//        }

        if(_quit) {
            return;
        }

        // Make copy of function, in case setCallback() gets called during async stuff.
        if(_fnAsync) {
            _q.pushPrioritisedEvent<RunFunctionEventAsync>(_requestingServiceId, getPriority(), _fnAsync);
        } else {
            _fn();
        }

        if(!_fireOnce) {
            // TODO instead of relying on UringResponseEvent, create a decltype(_fnAsync) event directly.
            auto *sqe = _q.getSqeWithData(_requestingServiceId, createNewHandler());
            io_uring_prep_timeout(sqe, &_timespec, 0, 0);
            _uringTimerId = sqe->user_data;
        }
    };
}
