#include <ichor/DependencyManager.h>

void Ichor::DependencyManager::handleEvent(RemoveEventInterceptorEvent &evt, ProcessEventContext &) {
    INTERNAL_DEBUG("RemoveEventInterceptorEvent {} {}", evt.id, evt.priority);

    // key.id = service id, key.type == event id
    auto const existingHandlers = _eventInterceptors.find(evt.eventType);
    if (existingHandlers != end(_eventInterceptors)) [[likely]] {
        std::erase_if(existingHandlers->second, [&evt](const EventInterceptInfo &info) noexcept {
            return info.interceptorId == evt.interceptorId;
        });
    }
}
