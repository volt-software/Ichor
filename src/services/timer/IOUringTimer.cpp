#include <ichor/services/timer/IOUringTimer.h>
#include <ichor/events/RunFunctionEvent.h>


using namespace std::chrono_literals;

Ichor::v1::IOUringTimer::IOUringTimer(Ichor::ScopedServiceProxy<IIOUringQueue*> queue, uint64_t timerId, ServiceIdType svcId) noexcept : _q(queue), _timerId(timerId), _requestingServiceId(svcId) {
    INTERNAL_IO_DEBUG("[{:L}][{}] IOUringTimer for {}", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), __LINE__, _requestingServiceId);

    if constexpr(DO_INTERNAL_DEBUG || DO_HARDENING) {
        if (_timerId > std::numeric_limits<unsigned int>::max()) {
            fmt::println("timerId overflow for io_uring");
            std::terminate();
        }
    }

    _multishotCapable = _q->getKernelVersion() >= Version{6, 4, 0};
}

bool Ichor::v1::IOUringTimer::startTimer() {
    return startTimer(false);
}

bool Ichor::v1::IOUringTimer::startTimer(bool fireImmediately) {
    if(!_fn && !_fnAsync) [[unlikely]] {
        fmt::println("No callback set.");
        std::terminate();
    }

    if(_state != TimerState::STOPPED) {
        return false;
    }

    INTERNAL_IO_DEBUG("[{:L}][{}] Starting IOUringTimer {} for {} with {} s {} ns timeout", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), __LINE__, _timerId, _requestingServiceId, _timespec.tv_sec, _timespec.tv_nsec);
    // A fire-once timer that fires immediately has nothing left to wait for, so it doesn't start running.
    if(!fireImmediately || !_fireOnce) {
        _runFinished = make_reference_counted<bool>(false);
        armTimeout();
        _state = TimerState::RUNNING;
    }

    if(fireImmediately) {
        if(_fnAsync) {
            _q->pushPrioritisedEvent<RunFunctionEventAsync>(_requestingServiceId, getPriority(), _fnAsync);
        } else {
            _fn();
        }
    }
    return true;
}

bool Ichor::v1::IOUringTimer::stopTimer(std::function<void(void)> cb) {
    INTERNAL_IO_DEBUG("Stopping timer {} for {} _uringTimerId 0x{:X} state {}", _timerId, _requestingServiceId, _uringTimerId, _state);
    if(_state == TimerState::RUNNING) {
        auto *sqe = _q->getSqeWithData(_requestingServiceId, createNewStopHandler());
        io_uring_prep_timeout_remove(sqe, _uringTimerId, 0);
        _state = TimerState::STOPPING;
    }
    if(_state == TimerState::STOPPING) {
        // called once the timeout has completed, after which the timer can be destroyed
        if(cb) {
            _quitCbs.emplace_back(std::move(cb));
        }
        return true;
    }
    if(cb) {
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
        auto *sqe = _q->getSqeWithData(_requestingServiceId, createNewUpdateHandler());
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

void Ichor::v1::IOUringTimer::armTimeout() {
    auto *sqe = _q->getSqeWithData(_requestingServiceId, createNewHandler());
    unsigned flags{};
    if (!_fireOnce && _multishotCapable) {
        flags = IORING_TIMEOUT_MULTISHOT;
    }
    io_uring_prep_timeout(sqe, &_timespec, 0, flags);
    _uringTimerId = sqe->user_data;
    INTERNAL_IO_DEBUG("IOUringTimer {} for {} _uringTimerId 0x{:X}", _timerId, _requestingServiceId, _uringTimerId);
}

std::function<void(io_uring_cqe *)> Ichor::v1::IOUringTimer::createNewHandler() noexcept {
    INTERNAL_IO_DEBUG("IOUringTimer {} for {} createNewHandler", _timerId, _requestingServiceId);
    return [this, runFinished = _runFinished](io_uring_cqe *cqe) {
        if(*runFinished) [[unlikely]] {
            // the timer may not exist anymore
            return;
        }
        bool const lastCompletion = (cqe->flags & IORING_CQE_F_MORE) == 0;

        if(cqe->res != -ETIME) {
            // removed by stopTimer (-ECANCELED) or an error, the timeout has ended
            if(cqe->res != -ECANCELED) {
                INTERNAL_IO_DEBUG("IOUringTimer {} for {} error {}", _timerId, _requestingServiceId, cqe->res);
            }
            if(lastCompletion) {
                finishStopping();
            }
            return;
        }
        INTERNAL_IO_DEBUG("IOUringTimer {} for {} cqe->res {}", _timerId, _requestingServiceId, cqe->res);

        if(_state == TimerState::STOPPING) {
            // Fired before the timeout_remove was processed. If this was the last completion, the remove will fail with -ENOENT.
            if(lastCompletion) {
                finishStopping();
            }
            return;
        }

        // Make copy of function, in case setCallback() gets called during async stuff.
        if(_fnAsync) {
            _q->pushPrioritisedEvent<RunFunctionEventAsync>(_requestingServiceId, getPriority(), _fnAsync);
        } else {
            _fn();
        }

        if(!lastCompletion) {
            // multishot timeout, keeps firing
            return;
        }

        // The timeout ended: one-shot timeouts (fire-once timers, kernels < 6.4), or a multishot timeout that ended early.
        if(_state == TimerState::STOPPING) {
            // the callback stopped the timer
            finishStopping();
        } else if(_fireOnce) {
            finishStopping();
        } else {
            armTimeout();
        }
    };
}

std::function<void(io_uring_cqe *)> Ichor::v1::IOUringTimer::createNewStopHandler() noexcept {
    INTERNAL_IO_DEBUG("IOUringTimer {} for {} createNewStopHandler", _timerId, _requestingServiceId);
    return [this, runFinished = _runFinished](io_uring_cqe *cqe) {
        if(cqe->res >= 0 || *runFinished) {
            // Removed, its -ECANCELED completion finishes stopping. Or the timeout already ended, in which case the timer may not exist anymore.
            return;
        }
        INTERNAL_IO_DEBUG("Couldn't stop timer {} for {} because {} uringTimerId 0x{:X}", _timerId, _requestingServiceId, cqe->res, _uringTimerId);
        if(cqe->res == -ENOENT || cqe->res == -EALREADY) {
            // -ENOENT: a multishot timeout that fired and is not re-armed yet, or the last completion of the timeout hasn't been processed yet.
            // -EALREADY: it is firing right now. Retry until either the remove succeeds or the timeout ends.
            auto *sqe = _q->getSqeWithData(_requestingServiceId, createNewStopHandler());
            io_uring_prep_timeout_remove(sqe, _uringTimerId, 0);
            return;
        }
        std::terminate();
    };
}

std::function<void(io_uring_cqe *)> Ichor::v1::IOUringTimer::createNewUpdateHandler() noexcept {
    INTERNAL_IO_DEBUG("IOUringTimer {} for {} createNewUpdateHandler", _timerId, _requestingServiceId);
    return [this, runFinished = _runFinished](io_uring_cqe *cqe) {
        if(cqe->res >= 0 || *runFinished) {
            // Updated, or the timeout already ended, in which case the timer may not exist anymore.
            return;
        }
        INTERNAL_IO_DEBUG("Couldn't update timer {} for {} because {} uringTimerId 0x{:X}", _timerId, _requestingServiceId, cqe->res, _uringTimerId);
        if(cqe->res == -ENOENT || cqe->res == -EALREADY) {
            // Same transient cases as for stopping. When stopping, there's no need to update anymore.
            if(_state == TimerState::RUNNING) {
                auto *sqe = _q->getSqeWithData(_requestingServiceId, createNewUpdateHandler());
                io_uring_prep_timeout_update(sqe, &_timespec, _uringTimerId, 0);
            }
            return;
        }
        std::terminate();
    };
}

void Ichor::v1::IOUringTimer::finishStopping() {
    auto quitCbs = std::move(_quitCbs);
    _quitCbs.clear();
    _state = TimerState::STOPPED;
    // From here on the timer can be destroyed, handlers of requests still in flight check this before touching it.
    *_runFinished = true;
    for(auto &cb : quitCbs) {
        cb();
    }
}
