#pragma once

#include <ichor/services/timer/ITimer.h>
#include <ichor/services/timer/TimerRef.h>
#include <ichor/services/timer/IInternalTimerFactory.h>
#include <iterator>
#include <algorithm>

namespace Ichor::v1 {
    extern std::atomic<uint64_t> _timerIdCounter;
    template <typename TIMER, typename QUEUE>
    class TimerFactory final : public ITimerFactory, public Ichor::Detail::v1::InternalTimerFactory, public AdvancedService<TimerFactory<TIMER, QUEUE>> {
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

        [[nodiscard]] TimerRef createTimer() final {
            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if(!AdvancedService<TimerFactory<TIMER, QUEUE>>::isStarted()) {
                    std::terminate();
                }
                if(_stoppingTimers) {
                    std::terminate();
                }
            }
            auto id = _timerIdCounter.fetch_add(1, std::memory_order_relaxed);
            _timers.emplace_back(*_q, id, _requestingSvcId);
            return TimerRef{*this, id};
        }

        [[nodiscard]] tl::optional<NeverNull<ITimer*>> getTimerById(uint64_t timerId) noexcept final {
            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if(!AdvancedService<TimerFactory<TIMER, QUEUE>>::isStarted()) {
                    std::terminate();
                }
                if(_stoppingTimers) {
                    std::terminate();
                }
            }

            auto it = std::find_if(_timers.begin(), _timers.end(), [timerId](TIMER &timer) {
                return timer.getTimerId() == timerId;
            });

            if(it == _timers.end()) {
                return {};
            }
            return &(*it);
        }

        bool destroyTimerIfStopped(uint64_t timerId) final {
            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if(!AdvancedService<TimerFactory<TIMER, QUEUE>>::isStarted()) {
                    std::terminate();
                }
            }

            auto erased = std::erase_if(_timers, [timerId](TIMER const &timer) {
                return timer.getTimerId() == timerId && timer.getState() != TimerState::RUNNING;
            });

            return erased > 0;
        }

        [[nodiscard]] VectorView<ITimer> getCreatedTimers() const noexcept final {
            return VectorView<ITimer>{_timers};
        }

        [[nodiscard]] virtual ServiceIdType getRequestingServiceId() const noexcept final {
            return _requestingSvcId;
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
                timer.stopTimer([this]() {
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

        std::vector<TIMER> _timers;
        ServiceIdType _requestingSvcId{};
        QUEUE *_q;
        std::size_t _stoppedTimers{};
        AsyncManualResetEvent _quitEvt;
        bool _stoppingTimers{};
    };
}
