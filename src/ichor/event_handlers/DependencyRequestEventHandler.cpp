#include <ichor/DependencyManager.h>

void Ichor::DependencyManager::handleEvent(DependencyRequestEvent &evt, ProcessEventContext &ctx) {
    INTERNAL_DEBUG("DependencyRequestEvent {} {} {} {}", evt.id, evt.priority, evt.originatingService, evt.dependency.getInterfaceName());

    auto const trackers = _dependencyRequestTrackers.find(evt.dependency.interfaceNameHash);
    if (trackers == end(_dependencyRequestTrackers)) {
        return;
    }

    v1::ReferenceCountedPointer<Event> refEvt;
    for (DependencyTrackerInfo const &info : trackers->second) {
        auto gen = info.trackFunc(evt);
        gen.set_service_id(info.svcId);
        gen.set_priority(evt.priority);
        auto it = gen.begin();

        if(!it.get_finished()) {
            if(!refEvt.has_value()) {
                refEvt = std::move(ctx.uniqueEvt);
                ctx.evt = refEvt.get();
            }
            _scopedGenerators.emplace(it.get_promise_id(), ScopedGenerator{std::make_unique<AsyncGenerator<IchorBehaviour>>(std::move(gen)), refEvt});
            // _scopedEvents.emplace(it.get_promise_id(), refEvt);
        }
    }
}
