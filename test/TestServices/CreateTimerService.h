#pragma once

#include <ichor/services/timer/ITimerFactory.h>

using namespace Ichor;
using namespace Ichor::v1;

extern std::atomic<uint64_t> evtGate;

template<uint64_t requestAmount>
class CreateTimerService final {
public:
    explicit CreateTimerService(ScopedServiceProxy<ITimerFactory> factory) {
        constexpr_for<static_cast<size_t>(0), requestAmount, static_cast<size_t>(1)>([this, factory](auto i) {
            _timers.emplace_back(factory->createTimer());
            evtGate++;
            fmt::println("createTimerService<{}> {}", requestAmount, static_cast<size_t>(i));
        });
    }

private:
    // The timers only exist as long as their TimerRefs
    std::vector<TimerRef> _timers;
};
