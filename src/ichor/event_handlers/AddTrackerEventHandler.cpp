#include <ichor/DependencyManager.h>

void Ichor::DependencyManager::handleEvent(AddTrackerEvent &evt, ProcessEventContext &) {
    INTERNAL_DEBUG("AddTrackerEvent {} {}", evt.id, evt.priority);

    if(auto const trackers = _dependencyRequestTrackers.find(evt.interfaceNameHash); trackers != _dependencyRequestTrackers.end()) {
        for(auto &tracker : trackers->second) {
            if(tracker.svcId != evt.originatingService) {
                continue;
            }

            std::vector<v1::ReferenceCountedPointer<DependencyRequestEvent>> requests{};
            for(auto const &[key, mgr] : _services) {
                auto const *depRegistry = mgr->getDependencyRegistry();
                // ICHOR_LOG_ERROR(_logger, "register svcId {} dm {}", mgr->serviceId(), _id);

                // only DependencyLifecycleManager has a non-nullptr value. Other Lifecyclemanagers return nullptr because they don't request dependencies.
                if(depRegistry == nullptr) {
                    continue;
                }

                for (auto const &[interfaceHash, registration] : depRegistry->_registrations) {
                    if(interfaceHash == evt.interfaceNameHash) {
                        auto const &props = std::get<tl::optional<Properties>>(registration);
                        requests.emplace_back(0u, mgr->serviceId(), std::min(mgr->getPriority(), INTERNAL_DEPENDENCY_EVENT_PRIORITY), std::get<Dependency>(registration), props.has_value() ? &props.value() : tl::optional<Properties const *>{});
                    }
                }
            }

            for(auto& request : requests) {
                auto gen = tracker.trackFunc(*request);
                gen.set_service_id(tracker.svcId);
                gen.set_priority(evt.priority);
                auto it = gen.begin();

                if(!it.get_finished()) {
                    _scopedGenerators.emplace(it.get_promise_id(), ScopedGenerator{std::make_unique<AsyncGenerator<IchorBehaviour>>(std::move(gen)), std::move(request)});
                    // _scopedEvents.emplace(it.get_promise_id(), std::move(request));
                }
            }

            break;
        }
    }
}
