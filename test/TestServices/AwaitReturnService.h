#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/events/Event.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>

using namespace Ichor;

extern std::unique_ptr<Ichor::AsyncManualResetEvent> _evt;

struct AwaitNoCopy{
    AwaitNoCopy() {
        countConstructed++;
    }
    ~AwaitNoCopy() noexcept {
        countDestructed++;
    }
    AwaitNoCopy(AwaitNoCopy const &) = delete;
    AwaitNoCopy(AwaitNoCopy&& o) noexcept {
        countMoved++;
    }
    AwaitNoCopy& operator=(AwaitNoCopy const &) = delete;
    AwaitNoCopy& operator=(AwaitNoCopy&& o) noexcept = delete;

    static uint64_t countConstructed;
    static uint64_t countDestructed;
    static uint64_t countMoved;
};

struct AwaitReturnService final : public AdvancedService<AwaitReturnService> {
    AwaitReturnService() = default;
    ~AwaitReturnService() final = default;

    AsyncGenerator<AwaitNoCopy> Await() {
        co_await *_evt;
        co_return AwaitNoCopy{};
    }

    Task<AwaitNoCopy> AwaitTask() {
        co_await *_evt;
        co_return AwaitNoCopy{};
    }

    uint64_t count{};
};
