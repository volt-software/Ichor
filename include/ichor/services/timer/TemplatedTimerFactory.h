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

        ITimer& createTimer() final {
            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (!AdvancedService<TimerFactory<TIMER, QUEUE>>::isStarted()) {
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
                return timer->getTimerId() == timerId && !timer->running();
            });

            return erased > 0;
        }

        void stopAllTimers() noexcept final {
            for(auto& timer : _timers) {
                timer->stopTimer();
            }
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
    };
}
