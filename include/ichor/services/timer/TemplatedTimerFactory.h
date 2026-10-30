#pragma once

#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/timer/ITimer.h>
#include <ichor/services/timer/TimerRef.h>
#include <ichor/services/timer/IInternalTimerFactory.h>
#include <iterator>
#include <algorithm>
#include <memory>
#include <vector>
#include <ichor/ScopedServiceProxy.h>
#include <ichor/stl/Spans.h>

namespace Ichor::v1 {
    extern std::atomic<uint64_t> _timerIdCounter;
    template <typename TIMER, typename QUEUE>
    class TimerFactory final : public ITimerFactory, public Ichor::v1::Detail::InternalTimerFactory, public AdvancedService<TimerFactory<TIMER, QUEUE>> {
    public:
        TimerFactory(DependencyRegister &reg, Properties props) : AdvancedService<TimerFactory<TIMER, QUEUE>>(std::move(props)),
                _requestingSvcId(any_cast<ServiceIdType>(AdvancedService<TimerFactory<TIMER, QUEUE>>::getProperties()["requestingSvcId"])) {
            reg.registerDependency<QUEUE>(this, DependencyFlags::REQUIRED);
        }
        ~TimerFactory() final {
            for(auto &state : _states) {
                state->timer = nullptr;
            }
            // Destroying the timers can destroy TimerRefs held by their callbacks, which then don't touch _timers anymore.
            auto timers = std::move(_timers);
        }

        Task<void> stop() override {
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
            auto timer = std::make_unique<TIMER>(_q, id, _requestingSvcId);
            auto state = std::make_shared<Detail::TimerRefState>(*timer, *this, id);
            _timers.emplace_back(std::move(timer));
            _states.emplace_back(state);
            return TimerRef{std::move(state)};
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

            auto idx = findTimer(timerId);

            if(idx == _timers.size()) {
                return {};
            }
            return _timers[idx].get();
        }

        bool destroyTimerIfStopped(uint64_t timerId) final {
            if(_stoppingTimers) {
                // E.g. from a stop callback while the factory stops. stopAllTimers counts on the timers staying around until they all
                // stopped, they get destroyed together with the factory.
                return false;
            }

            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if(!AdvancedService<TimerFactory<TIMER, QUEUE>>::isStarted()) {
                    std::terminate();
                }
            }

            auto idx = findTimer(timerId);

            // not while STOPPING, requests of the timer can still be in flight
            if(idx == _timers.size() || _timers[idx]->getState() != TimerState::STOPPED) {
                return false;
            }

            destroyTimer(idx);
            return true;
        }

        void releaseTimer(uint64_t timerId) noexcept final {
            // Not started: TimerRefs get destroyed while the requesting service stops, which is while this factory is already STOPPING
            // but before stopAllTimers. Either way, stopAllTimers stops it and it gets destroyed together with the factory.
            if(_stoppingTimers || !AdvancedService<TimerFactory<TIMER, QUEUE>>::isStarted()) {
                return;
            }

            auto idx = findTimer(timerId);

            if(idx == _timers.size()) {
                return;
            }

            // Also when already stopped: the stop callbacks of the thread based Timer run from an event, the callback is then called after
            // those instead of right away. Destroying the timer before them would let this factory stop without waiting for them.
            // That event can run after this factory is destroyed, which also destroys the state.
            _timers[idx]->stopTimer([weakState = std::weak_ptr<Detail::TimerRefState>{_states[idx]}]() {
                auto state = weakState.lock();
                if(state && state->timer != nullptr) {
                    // only this factory creates these states
                    static_cast<TimerFactory*>(state->factory)->destroyReleasedTimer(state->timerId);
                }
            });
        }

        [[nodiscard]] VectorView<ITimer> getCreatedTimers() const noexcept final {
            return VectorView<ITimer>{_timers};
        }

        [[nodiscard]] ServiceIdType getRequestingServiceId() const noexcept final {
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

        void addDependencyInstance(Ichor::ScopedServiceProxy<QUEUE*> q, IService&) noexcept {
            _q = std::move(q);
        }

        void removeDependencyInstance(Ichor::ScopedServiceProxy<QUEUE*>, IService&) noexcept {
            _q.reset();
        }

    private:
        /// Index of the timer in _timers and _states, _timers.size() when not found
        [[nodiscard]] std::size_t findTimer(uint64_t timerId) const noexcept {
            auto it = std::find_if(_timers.begin(), _timers.end(), [timerId](std::unique_ptr<TIMER> const &timer) {
                return timer->getTimerId() == timerId;
            });
            return static_cast<std::size_t>(std::distance(_timers.begin(), it));
        }

        /// Like destroyTimerIfStopped, but for a released timer, which may be stopping while this factory isn't started anymore
        void destroyReleasedTimer(uint64_t timerId) noexcept {
            if(_stoppingTimers || !AdvancedService<TimerFactory<TIMER, QUEUE>>::isStarted()) {
                return;
            }

            auto idx = findTimer(timerId);

            if(idx != _timers.size() && _timers[idx]->getState() == TimerState::STOPPED) {
                destroyTimer(idx);
            }
        }

        void destroyTimer(std::size_t idx) noexcept {
            // Erase before destroying, the destructor of the timer may destroy TimerRefs held by its callbacks, which end up in releaseTimer.
            auto timer = std::move(_timers[idx]);
            _states[idx]->timer = nullptr;
            _timers.erase(_timers.begin() + static_cast<std::ptrdiff_t>(idx));
            _states.erase(_states.begin() + static_cast<std::ptrdiff_t>(idx));
        }

        // Timers are referenced by their own threads/in-flight requests while running and by TimerRefs, so they need stable addresses.
        // Not a vector of structs, so that getCreatedTimers() can return a view over it.
        std::vector<std::unique_ptr<TIMER>> _timers;
        // The state shared with the TimerRefs of the timer at the same index in _timers
        std::vector<std::shared_ptr<Detail::TimerRefState>> _states;
        ServiceIdType _requestingSvcId{};
        Ichor::ScopedServiceProxy<QUEUE*> _q;
        std::size_t _stoppedTimers{};
        AsyncManualResetEvent _quitEvt;
        bool _stoppingTimers{};
    };
}
