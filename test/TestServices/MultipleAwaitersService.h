#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/events/Event.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/coroutines/AsyncAutoResetEvent.h>

using namespace Ichor;

extern std::unique_ptr<Ichor::AsyncAutoResetEvent> _autoEvt;

struct MultipleAwaitService final : public AdvancedService<MultipleAwaitService> {
    MultipleAwaitService() = default;
    ~MultipleAwaitService() final = default;

    Task<tl::expected<void, Ichor::StartError>> start() final {
        GetThreadLocalEventQueue().pushEvent<RunFunctionEventAsync>(getServiceId(), [this](DependencyManager &dm) -> AsyncGenerator<IchorBehaviour> {
            co_await *_autoEvt;
            count++;
            co_return {};
        });
        GetThreadLocalEventQueue().pushEvent<RunFunctionEventAsync>(getServiceId(), [this](DependencyManager &dm) -> AsyncGenerator<IchorBehaviour> {
            co_await *_autoEvt;
            count++;
            co_return {};
        });
        co_return {};
    }

    Task<void> stop() final {
        co_return;
    }

    uint64_t count{};
};
