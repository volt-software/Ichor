#pragma once

#include <ichor/services/timer/ITimerFactory.h>

using namespace Ichor;
using namespace Ichor::v1;

extern std::atomic<uint64_t> evtGate;

template<uint64_t requestAmount>
class CreateTimerService final {
public:
    CreateTimerService(ITimerFactory *factory) {
        constexpr_for<(size_t)0, requestAmount, (size_t)1>([factory](auto i) {
            auto timer = factory->createTimer();
            evtGate++;
            fmt::println("createTimerService<{}> {}", requestAmount, (size_t)i);
        });
    }
};
