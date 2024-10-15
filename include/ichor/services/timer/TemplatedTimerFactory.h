#pragma once

#include <ichor/services/timer/ITimer.h>

namespace Ichor {
    extern std::atomic<uint64_t> _timerIdCounter;
    template <typename TIMER, typename QUEUE>
    class TimerFactory final : public ITimerFactory, public Detail::InternalTimerFactory, public AdvancedService<TimerFactory<TIMER, QUEUE>> {
    public:
        TimerFactory(DependencyRegister &reg, Properties props) : AdvancedService<TimerFactory<TIMER, QUEUE>>(std::move(props)) {
            _requestingSvcId = any_cast<ServiceIdType>(AdvancedService<TimerFactory<TIMER, QUEUE>>::getProperties()["requestingSvcId"]);
            reg.registerDependency<QUEUE>(this, DependencyFlags::REQUIRED);
        }
        ~TimerFactory() final = default;


        Task<void> stop() {
            INTERNAL_IO_DEBUG("TimerFactory<{}, {}> {} stopping {}", typeName<TIMER>(), typeName<QUEUE>(), AdvancedService<TimerFactory<TIMER, QUEUE>>::getServiceId(), _stoppingTimers);
            if(!_stoppingTimers) {
                co_await stopAllTimers();
            } else {
                co_await _quitEvt;
            }

            INTERNAL_IO_DEBUG("TimerFactory<{}, {}> {} stopped", typeName<TIMER>(), typeName<QUEUE>(), AdvancedService<TimerFactory<TIMER, QUEUE>>::getServiceId());

            co_return;
        }

        [[nodiscard]] ITimer& createTimer() final {
            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (!AdvancedService<TimerFactory<TIMER, QUEUE>>::isStarted()) {
                    std::terminate();
                }
                if (_stoppingTimers) {
                    std::terminate();
                }
            }
            return *(_timers.emplace_back(new TIMER(*_q, _timerIdCounter.fetch_add(1, std::memory_order_relaxed), _requestingSvcId)));
        }

        bool destroyTimerIfStopped(uint64_t timerId) final {
            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if(!AdvancedService<TimerFactory<TIMER, QUEUE>>::isStarted()) {
                    std::terminate();
                }
            }

            auto erased = std::erase_if(_timers, [timerId](std::unique_ptr<TIMER> const &timer) {
                return timer->getTimerId() == timerId && timer->getState() != TimerState::RUNNING;
            });

            return erased > 0;
        }

        // TODO create a wrapper of iterator class to create a non-allocating version of this.
        [[nodiscard]] std::vector<NeverNull<ITimer*>> getCreatedTimers() const noexcept final {
            std::vector<NeverNull<ITimer*>> ret;
            ret.reserve(_timers.size());

            for(auto &tmr : _timers) {
                ret.emplace_back(tmr.get());
            }

            return ret;
        }

        Task<void> stopAllTimers() noexcept final {
            INTERNAL_IO_DEBUG("TimerFactory<{}, {}> {} stopAllTimers for {} total {}", typeName<TIMER>(), typeName<QUEUE>(), AdvancedService<TimerFactory<TIMER, QUEUE>>::getServiceId(), _requestingSvcId, _timers.size());

            if (_stoppingTimers) {
                co_await _quitEvt;
                co_return;
            }

            _stoppingTimers = true;
            _stoppedTimers = 0;

            for(auto& timer : _timers) {
                timer->stopTimer([this]() {
                    _stoppedTimers++;
                    INTERNAL_IO_DEBUG("TimerFactory<{}, {}> {} for {} _stoppedTimers {} {}", typeName<TIMER>(), typeName<QUEUE>(), AdvancedService<TimerFactory<TIMER, QUEUE>>::getServiceId(), _requestingSvcId, _stoppedTimers, _timers.size());
                    if(_stoppedTimers == _timers.size()) {
                        _quitEvt.set();
                    }
                });
            }

            // while it might be slightly more performant to skip await entirely, _quitEvt is used in multiple places so we want to ensure everything works correctly.
            if(_timers.empty()) {
                _quitEvt.set();
            } else {
                co_await _quitEvt;
            }
            INTERNAL_IO_DEBUG("TimerFactory<{}, {}> {} stopAllTimers for {} done", typeName<TIMER>(), typeName<QUEUE>(), AdvancedService<TimerFactory<TIMER, QUEUE>>::getServiceId(), _requestingSvcId);

            co_return;
        }

        void addDependencyInstance(QUEUE &q, IService&) noexcept {
            _q = &q;
        }

        void removeDependencyInstance(QUEUE&, IService&) noexcept {
            _q = nullptr;
        }

        std::vector<std::unique_ptr<TIMER>> _timers;
        ServiceIdType _requestingSvcId{};
        QUEUE *_q;
        std::size_t _stoppedTimers{};
        AsyncManualResetEvent _quitEvt;
        bool _stoppingTimers{};
    };
}
