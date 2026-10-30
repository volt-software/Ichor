#include <ichor/DependencyManager.h>

void Ichor::DependencyManager::handleEvent(ContinuableStartEvent &evt, ProcessEventContext &ctx) {
    INTERNAL_DEBUG("ContinuableStartEvent {} {} {}", evt.promiseId, evt.id, evt.priority);
    auto const genIt = _scopedGenerators.find(evt.promiseId);

    if (genIt == _scopedGenerators.end()) {
        return;
    }

    INTERNAL_DEBUG("ContinuableStartEvent {}", genIt->second.generator->done());

    StartBehaviour it_ret{};
    if (!genIt->second.generator->done()) {
        auto it = genIt->second.generator->begin_interface();
        INTERNAL_DEBUG("ContinuableStartEvent it {} {} {}", it->get_finished(), it->get_op_state(), it->get_promise_state());

        if (!it->get_finished()) {
            if constexpr (DO_INTERNAL_DEBUG || DO_INTERNAL_COROUTINE_DEBUG) {
                if (!it->get_has_suspended()) [[unlikely]] {
                    INTERNAL_DEBUG("{}", it->get_promise_id());
                    std::terminate();
                }
            }
            return;
        }

        it_ret = static_cast<Ichor::v1::Detail::AsyncGeneratorBeginOperation<StartBehaviour>*>(it.get())->get_value();
    } else {
        it_ret = static_cast<AsyncGenerator<StartBehaviour>*>(genIt->second.generator.get())->get_value();
    }

    INTERNAL_DEBUG("ContinuableStartEvent removed {} {} {}", it_ret, evt.promiseId, _scopedGenerators.size() - 1);

    auto origEvtType = genIt->second.event->get_type();
    auto *origEvt = genIt->second.event.get();

    // the event whose handling started the coroutine finishes in the file of its own handler
    switch(origEvtType) {
        case StartServiceEvent::TYPE: finishAsyncEvent(static_cast<StartServiceEvent &>(*origEvt), evt, it_ret, ctx); break;
        case StopServiceEvent::TYPE: finishAsyncEvent(static_cast<StopServiceEvent &>(*origEvt), evt, it_ret, ctx); break;
        case DependencyOnlineEvent::TYPE: finishAsyncEvent(static_cast<DependencyOnlineEvent &>(*origEvt), evt, it_ret, ctx); break;
        case ContinuableDependencyOfflineEvent::TYPE: finishAsyncEvent(static_cast<ContinuableDependencyOfflineEvent &>(*origEvt), evt, it_ret, ctx); break;
        default:
            ICHOR_EMERGENCY_LOG2(_logger, "{}", origEvt->get_name());
            ICHOR_EMERGENCY_LOG1(_logger, "Something went wrong, perhaps your coroutine uses the internal StartBehaviour as a return type. Otherwise, please file a bug in Ichor.");
            std::terminate();
    }

    handleEventCompletion(*origEvt);
    _scopedGenerators.erase(genIt);

    // A stopped service might be the last one a QuitEvent waits for. This has to come after the erase, as there is no quitting while generators exist.
    if(origEvtType == StopServiceEvent::TYPE) {
        checkIfCanQuit(ctx.allEventInterceptorsCopy, ctx.eventInterceptorsCopy);
    }
}
