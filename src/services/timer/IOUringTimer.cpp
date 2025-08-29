#include <ichor/services/timer/IOUringTimer.h>
#include <ichor/events/RunFunctionEvent.h>


using namespace std::chrono_literals;

static std::function<void(io_uring_cqe *)> createNewStopHandler(Ichor::IIOUringQueue *q, Ichor::ServiceIdType requestingServiceId, uint64_t timerId, uint64_t uringTimerId) noexcept {
    INTERNAL_IO_DEBUG("IOUringTimer {} for {} createNewStopHandler", timerId, requestingServiceId);
    return [q, requestingServiceId, timerId, uringTimerId](io_uring_cqe *cqe) {
        if(cqe->res < 0) {
            INTERNAL_IO_DEBUG("Couldn't stop timer {} for {} because {} uringTimerId 0x{:X}", timerId, requestingServiceId, cqe->res, uringTimerId);
            if(cqe->res == -ENOENT || cqe->res == -EALREADY) {
                auto *sqe = q->getSqeWithData(requestingServiceId, createNewStopHandler(q, requestingServiceId, timerId, uringTimerId));
                io_uring_prep_timeout_remove(sqe, uringTimerId, 0);
            } else {
                std::terminate();
            }
        } else {
            INTERNAL_IO_DEBUG("Stopped timer {} for {} because {} uringTimerId 0x{:X}", timerId, requestingServiceId, cqe->res, uringTimerId);
        }
    };
}

Ichor::v1::IOUringTimer::IOUringTimer(IIOUringQueue& queue, uint64_t timerId, uint64_t svcId) noexcept : _q(queue), _timerId(timerId), _requestingServiceId(svcId) {
    INTERNAL_IO_DEBUG("[{:L}][{}] IOUringTimer for {}", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), __LINE__, _requestingServiceId);

    if constexpr(DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (_timerId > std::numeric_limits<unsigned int>::max()) {
            fmt::println("timerId overflow for io_uring");
            std::terminate();
        }
    }

    _multishotCapable = _q.getKernelVersion() >= Version{6, 4, 0};
}

bool Ichor::v1::IOUringTimer::startTimer() {
    return startTimer(false);
}

bool Ichor::v1::IOUringTimer::startTimer(bool fireImmediately) {
    if(!_fn && !_fnAsync) [[unlikely]] {
        fmt::println("No callback set.");
        std::terminate();
    }

    if(_state == TimerState::STOPPED) {
        INTERNAL_IO_DEBUG("[{:L}][{}] Starting IOUringTimer {} for {} with {} s {} ns timeout", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), __LINE__, _timerId, _requestingServiceId, _timespec.tv_sec, _timespec.tv_nsec);
        if(!fireImmediately || !_fireOnce) {
            auto *sqe = _q.getSqeWithData(_requestingServiceId, createNewHandler());
            unsigned flags{};
            if (!_fireOnce && _multishotCapable) {
                flags = IORING_TIMEOUT_MULTISHOT;
            }
            io_uring_prep_timeout(sqe, &_timespec, 0, flags);
            _uringTimerId = sqe->user_data;
            INTERNAL_IO_DEBUG("IOUringTimer {} for {} _uringTimerId 0x{:X}", _timerId, _requestingServiceId, _uringTimerId);
        }

        _state = TimerState::RUNNING;

        if(fireImmediately) {
            if(_fnAsync) {
                _q.pushPrioritisedEvent<RunFunctionEventAsync>(_requestingServiceId, getPriority(), _fnAsync);
            } else {
                _fn();
            }
        }
        return true;
    }
    return false;
}

bool Ichor::v1::IOUringTimer::stopTimer(std::function<void(void)> cb) {
    INTERNAL_IO_DEBUG("Stopping timer {} for {} _uringTimerId 0x{:X} state {}", _timerId, _requestingServiceId, _uringTimerId, _state);
    if(_state == TimerState::RUNNING) {
        auto *sqe = _q.getSqeWithData(_requestingServiceId, createNewStopHandler(&_q, _requestingServiceId, _timerId, _uringTimerId));
        io_uring_prep_timeout_remove(sqe, _uringTimerId, 0);
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

Ichor::v1::TimerState Ichor::v1::IOUringTimer::getState() const noexcept {
    return _state;
};

void Ichor::v1::IOUringTimer::setCallbackAsync(std::function<AsyncGenerator<IchorBehaviour>()> fn) {
    if(_state != TimerState::STOPPED) {
        std::terminate();
    }

    _fnAsync = std::move(fn);
    _fn = {};
}

void Ichor::v1::IOUringTimer::setCallback(std::function<void()> fn) {
    if(_state != TimerState::STOPPED) {
        std::terminate();
    }

    _fnAsync = {};
    _fn = std::move(fn);
}

void Ichor::v1::IOUringTimer::setInterval(uint64_t nanoseconds) noexcept {
    if constexpr(DO_INTERNAL_DEBUG || DO_HARDENING) {
        if(nanoseconds > std::numeric_limits<decltype(_timespec.tv_nsec)>::max()) {
            fmt::println("timer {} for {} given nanoseconds {} is greater than allowed by type {}", _timerId, _requestingServiceId, nanoseconds, std::numeric_limits<decltype(_timespec.tv_nsec)>::max());
            std::terminate();
        }
    }

    _timespec.tv_nsec = static_cast<decltype(_timespec.tv_nsec)>(nanoseconds);

    if(_state == TimerState::RUNNING) {
        INTERNAL_IO_DEBUG("Updating timer {} for {} _uringTimerId {:X}", _timerId, _requestingServiceId, _uringTimerId);
        auto *sqe = _q.getSqeWithData(_requestingServiceId, createNewUpdateHandler());
        io_uring_prep_timeout_update(sqe, &_timespec, _uringTimerId, 0);
    }
}


void Ichor::v1::IOUringTimer::setPriority(uint64_t priority) noexcept {
    _priority = priority;
}

uint64_t Ichor::v1::IOUringTimer::getPriority() const noexcept {
    return _priority;
}

void Ichor::v1::IOUringTimer::setFireOnce(bool fireOnce) noexcept {
    _fireOnce = fireOnce;
}

bool Ichor::v1::IOUringTimer::getFireOnce() const noexcept {
    return _fireOnce;
}

uint64_t Ichor::v1::IOUringTimer::getTimerId() const noexcept {
    return _timerId;
}

Ichor::ServiceIdType Ichor::v1::IOUringTimer::getRequestingServiceId() const noexcept {
    return _requestingServiceId;
}

std::function<void(io_uring_cqe *)> Ichor::v1::IOUringTimer::createNewHandler() noexcept {
    INTERNAL_IO_DEBUG("IOUringTimer {} for {} createNewHandler", _timerId, _requestingServiceId);
    return [this](io_uring_cqe *cqe) {
        if((cqe->res <= 0 && cqe->res != -ETIME)) {
            if(cqe->res != -ECANCELED) {
                INTERNAL_IO_DEBUG("IOUringTimer {} for {} error {}", _timerId, _requestingServiceId, cqe->res);
            }
            for(auto &cb : _quitCbs) {
                cb();
            }
            _state = TimerState::STOPPED;
            return;
        }
        INTERNAL_IO_DEBUG("IOUringTimer {} for {} cqe->res {}", _timerId, _requestingServiceId, cqe->res);

        if(_state == TimerState::STOPPING) {
            return;
        }

        // Make copy of function, in case setCallback() gets called during async stuff.
        if(_fnAsync) {
            _q.pushPrioritisedEvent<RunFunctionEventAsync>(_requestingServiceId, getPriority(), _fnAsync);
        } else {
            _fn();
        }

        if (!_fireOnce && !_multishotCapable) {
            auto *sqe = _q.getSqeWithData(_requestingServiceId, createNewHandler());
            io_uring_prep_timeout(sqe, &_timespec, 0, 0);
            _uringTimerId = sqe->user_data;
            INTERNAL_IO_DEBUG("IOUringTimer {} for {} _uringTimerId2 0x{:X}", _timerId, _requestingServiceId, _uringTimerId);
        }
    };
}

std::function<void(io_uring_cqe *)> Ichor::v1::IOUringTimer::createNewUpdateHandler() noexcept {
    INTERNAL_IO_DEBUG("IOUringTimer {} for {} createNewUpdateHandler", _timerId, _requestingServiceId);
    return [this](io_uring_cqe *cqe) {
        if(cqe->res < 0) {
            INTERNAL_IO_DEBUG("Couldn't update timer {} for {} because {} uringTimerId 0x{:X}", _timerId, _requestingServiceId, cqe->res, _uringTimerId);
            if(cqe->res == -ENOENT || cqe->res == -EALREADY) {
                auto *sqe = _q.getSqeWithData(_requestingServiceId, createNewUpdateHandler());
                io_uring_prep_timeout_update(sqe, &_timespec, _uringTimerId, 0);
            } else {
                std::terminate();
            }
        } else {
            INTERNAL_IO_DEBUG("Updated timer {} for {} because {} uringTimerId 0x{:X}", _timerId, _requestingServiceId, cqe->res, _uringTimerId);
        }
    };
}
