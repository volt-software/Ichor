#pragma once

#include <ichor/Service.h>
#include <ichor/events/Event.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/coroutines/AsyncAutoResetEvent.h>

using namespace Ichor;

extern std::unique_ptr<Ichor::AsyncAutoResetEvent> _autoEvt;

struct MultipleAwaitService final : public Service<MultipleAwaitService> {
    MultipleAwaitService() = default;
    ~MultipleAwaitService() final = default;

    AsyncGenerator<void> start() final {
        getManager().pushEvent<RunFunctionEvent>(getServiceId(), [this](DependencyManager &dm) -> AsyncGenerator<IchorBehaviour> {
            co_await *_autoEvt;
            count++;
            co_return {};
        });
        getManager().pushEvent<RunFunctionEvent>(getServiceId(), [this](DependencyManager &dm) -> AsyncGenerator<IchorBehaviour> {
            co_await *_autoEvt;
            count++;
            co_return {};
        });
        co_return;
    }

    AsyncGenerator<void> stop() final {
        co_return;
    }

    uint64_t count{};
};