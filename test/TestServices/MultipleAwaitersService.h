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

    StartBehaviour start() final {
        getManager().pushEvent<RunFunctionEvent>(getServiceId(), [this](DependencyManager &dm) -> AsyncGenerator<void> {
            co_await *_autoEvt;
            count++;
        });
        getManager().pushEvent<RunFunctionEvent>(getServiceId(), [this](DependencyManager &dm) -> AsyncGenerator<void> {
            co_await *_autoEvt;
            count++;
        });
        return StartBehaviour::SUCCEEDED;
    }

    StartBehaviour stop() final {
        return StartBehaviour::SUCCEEDED;
    }

    uint64_t count{};
};