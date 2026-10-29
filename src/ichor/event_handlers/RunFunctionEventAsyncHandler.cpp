#include <ichor/DependencyManager.h>
#include <ichor/events/RunFunctionEvent.h>

void Ichor::DependencyManager::handleEvent(RunFunctionEventAsync &evt, ProcessEventContext &ctx) {
    INTERNAL_DEBUG("RunFunctionEventAsync {} {}", evt.id, evt.priority);

    // Do not handle stale run function events
    if(evt.originatingService != ServiceIdType{0}) {
        auto const requestingServiceIt = _services.find(evt.originatingService);
        if(requestingServiceIt == end(_services)) {
            INTERNAL_DEBUG("Service {} not found", evt.originatingService);
            return;
        }
        if(requestingServiceIt->second->getServiceState() == ServiceState::INSTALLED) {
            INTERNAL_DEBUG("Service {}:{} not active", evt.originatingService, requestingServiceIt->second->implementationName());
            return;
        }
    }

    auto gen = evt.fun();
    gen.set_service_id(evt.originatingService);
    gen.set_priority(evt.priority);
    auto it = gen.begin();
    INTERNAL_DEBUG("state: {} {} {} {} {}", gen.done(), it.get_finished(), it.get_op_state(), it.get_promise_state(), it.get_promise_id());

    if (!it.get_finished()) {
        if constexpr (DO_INTERNAL_DEBUG) {
            if (!it.get_has_suspended()) [[unlikely]] {
                std::terminate();
            }
        }
        INTERNAL_DEBUG("contains2 {} {} {}", it.get_promise_id(), _scopedGenerators.contains(it.get_promise_id()),
                       _scopedGenerators.size() + 1);
        auto scopedIt = _scopedGenerators.emplace(it.get_promise_id(), ScopedGenerator{std::make_unique<AsyncGenerator<IchorBehaviour>>(std::move(gen)), std::move(ctx.uniqueEvt)});
        // auto scopedIt = _scopedEvents.emplace(it.get_promise_id(), std::move(ctx.uniqueEvt));
        ctx.evt = scopedIt.first->second.event.get();
    } else {
        if constexpr (DO_INTERNAL_DEBUG) {
            if (it.get_has_suspended()) [[unlikely]] {
                std::terminate();
            }
        }
    }
}
