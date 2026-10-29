#include <ichor/DependencyManager.h>

void Ichor::DependencyManager::handleEvent(RemoveEventHandlerEvent &evt, ProcessEventContext &) {
    INTERNAL_DEBUG("RemoveEventHandlerEvent {} {}", evt.id, evt.priority);

    // key.id = service id, key.type == event id
    auto const existingHandlers = _eventCallbacks.find(evt.key.type);
    if (existingHandlers != end(_eventCallbacks)) [[likely]] {
        std::erase_if(existingHandlers->second, [&evt](const EventCallbackInfo &info) noexcept {
            return info.listeningServiceId == ServiceIdType{evt.key.id};
        });
    }
}
