#include <ichor/DependencyManager.h>

void Ichor::DependencyManager::handleEvent(ContinuableEvent &evt, ProcessEventContext &ctx) {
    INTERNAL_DEBUG("ContinuableEvent {} {} {}", evt.promiseId, evt.id, evt.priority);
    auto const genIt = _scopedGenerators.find(evt.promiseId);

    if (genIt != _scopedGenerators.end()) {
        INTERNAL_DEBUG("ContinuableEvent2 {}", genIt->second.generator->done());

        if (!genIt->second.generator->done()) {
            auto it = genIt->second.generator->begin_interface();
            INTERNAL_DEBUG("ContinuableEvent it {} {} {}", it->get_finished(), it->get_op_state(), it->get_promise_state());

            if (!it->get_finished() && it->get_promise_state() != state::value_not_ready_consumer_active) {
                if constexpr (DO_INTERNAL_DEBUG || DO_INTERNAL_COROUTINE_DEBUG) {
                    if (!it->get_has_suspended()) [[unlikely]] {
                        INTERNAL_DEBUG("{}", it->get_promise_id());
                        std::terminate();
                    }
                }
            }

            if (it->get_finished()) {
                INTERNAL_DEBUG("removed1 {} size {}", evt.promiseId, _scopedGenerators.size() - 1);
                if(genIt->second.event.use_count() == 1) {
                    handleEventCompletion(*genIt->second.event);
                }

                {
                    auto id = genIt->second.generator->get_service_id();
                    INTERNAL_DEBUG("removed1 {} checking pending stop for {}", evt.promiseId, id);

                    auto pendingStopIt = _pendingStopsDueToCoroutine.find(id);

                    if(pendingStopIt != _pendingStopsDueToCoroutine.end()) {
                        INTERNAL_DEBUG("removed1 {} re-triggering StopServiceEvent for {}", evt.promiseId, id);

                        _eventQueue.pushPrioritisedEvent<StopServiceEvent>(pendingStopIt->second.originatingServiceId, std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, pendingStopIt->second.priority), id, pendingStopIt->second.removeAfter);
                        _pendingStopsDueToCoroutine.erase(pendingStopIt);
                    }
                }

                _scopedGenerators.erase(genIt);
            }
        } else {
            INTERNAL_DEBUG("removed2 {} size {}", evt.promiseId, _scopedGenerators.size() - 1);
            if(genIt->second.event.use_count() == 1) {
                handleEventCompletion(*genIt->second.event);
            }

            {
                auto id = genIt->second.generator->get_service_id();
                INTERNAL_DEBUG("removed2 {} checking pending stop for {}", evt.promiseId, id);

                auto pendingStopIt = _pendingStopsDueToCoroutine.find(id);

                if(pendingStopIt != _pendingStopsDueToCoroutine.end()) {
                    INTERNAL_DEBUG("removed2 {} re-triggering StopServiceEvent for {}", evt.promiseId, id);

                    _eventQueue.pushPrioritisedEvent<StopServiceEvent>(pendingStopIt->second.originatingServiceId, std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, pendingStopIt->second.priority), id, pendingStopIt->second.removeAfter);
                    _pendingStopsDueToCoroutine.erase(pendingStopIt);
                }
            }

            _scopedGenerators.erase(genIt);
        }
        checkIfCanQuit(ctx.allEventInterceptorsCopy, ctx.eventInterceptorsCopy);
    }
}
