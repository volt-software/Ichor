#include <ichor/DependencyManager.h>

void Ichor::DependencyManager::handleEvent(RemoveTrackerEvent &evt, ProcessEventContext &) {
    INTERNAL_DEBUG("RemoveTrackerEvent {} {}", evt.id, evt.priority);

    if(auto const trackers = _dependencyRequestTrackers.find(evt.interfaceNameHash); trackers != _dependencyRequestTrackers.end()) {
        std::erase_if(trackers->second, [&evt](DependencyTrackerInfo const &info) {
            return info.svcId == evt.originatingService;
        });
        if(trackers->second.empty()) {
            _dependencyRequestTrackers.erase(trackers);
        }
    }
}
