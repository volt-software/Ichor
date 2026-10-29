#include <ichor/DependencyManager.h>

void Ichor::DependencyManager::handleEvent(StartServiceEvent &evt, ProcessEventContext &ctx) {
    auto const toStartServiceIt = _services.find(evt.serviceId);

    if (toStartServiceIt == end(_services)) [[unlikely]] {
        INTERNAL_DEBUG( "Couldn't start service {}, missing from known services", evt.serviceId);

        if constexpr (DO_INTERNAL_DEBUG) {
            for(auto const &[svcId, svc] : _services) {
                INTERNAL_DEBUG( "service {}:{}", svcId, svc->implementationName());
            }
        }
        return;
    }

    auto *toStartService = toStartServiceIt->second.get();
    if (toStartService->getServiceState() == ServiceState::ACTIVE) {
        INTERNAL_DEBUG("StartServiceEvent service {}:{} already started", toStartService->serviceId(), toStartService->implementationName());
        return;
    }
    if (toStartService->getServiceState() == ServiceState::STARTING || toStartService->getServiceState() == ServiceState::INJECTING) {
        INTERNAL_DEBUG("StartServiceEvent service {}:{} already starting", toStartService->serviceId(), toStartService->implementationName());
        return;
    }

    INTERNAL_DEBUG("StartServiceEvent service {} {} {}:{}", evt.id, evt.priority, toStartService->serviceId(), toStartService->implementationName());
    auto gen = toStartService->start();
    gen.set_service_id(toStartService->serviceId());
    gen.set_priority(std::min(toStartService->getPriority(), INTERNAL_DEPENDENCY_EVENT_PRIORITY));
    auto it = gen.begin();

    if (!it.get_finished()) {
        if constexpr (DO_INTERNAL_DEBUG || DO_INTERNAL_COROUTINE_DEBUG) {
            if (!it.get_has_suspended()) [[unlikely]] {
                std::terminate();
            }
        }
        INTERNAL_DEBUG("StartServiceEvent contains {}:{} {} {} {}", toStartService->serviceId(), toStartService->implementationName(), it.get_promise_id(), _scopedGenerators.contains(it.get_promise_id()),
                       _scopedGenerators.size() + 1);
        auto scopedIt = _scopedGenerators.emplace(it.get_promise_id(), ScopedGenerator{std::make_unique<AsyncGenerator<StartBehaviour>>(std::move(gen)), std::move(ctx.uniqueEvt)});
        // auto scopedIt = _scopedEvents.emplace(it.get_promise_id(), std::move(ctx.uniqueEvt));
        ctx.evt = scopedIt.first->second.event.get();
        return;
    }

    if constexpr (DO_INTERNAL_DEBUG || DO_INTERNAL_COROUTINE_DEBUG) {
        if (it.get_has_suspended()) [[unlikely]] {
            INTERNAL_DEBUG("{}", it.get_promise_id());
            std::terminate();
        }
    }

    if(toStartService->getServiceState() == ServiceState::INSTALLED) [[unlikely]] {
        INTERNAL_DEBUG("StartServiceEvent service INSTALLED {}:{} {} {}", toStartService->serviceId(), toStartService->implementationName(), it.get_promise_id(), it.get_finished());
        return;
    }

    INTERNAL_DEBUG("StartServiceEvent finished {}:{} {}", toStartService->serviceId(), toStartService->implementationName(), it.get_promise_id());

    _eventQueue.pushPrioritisedEvent<DependencyOnlineEvent>(toStartService->serviceId(), std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt.priority));
    // If we're quitting, ensure newly started services are also stopped
    if(_quitEventReceived) {
        _eventQueue.pushPrioritisedEvent<StopServiceEvent>(toStartService->serviceId(), std::min(INTERNAL_DEPENDENCY_EVENT_PRIORITY, evt.priority), toStartService->serviceId(), true);
    }
}
