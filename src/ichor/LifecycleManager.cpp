#include <ichor/DependencyManager.h>

Ichor::AsyncGenerator<void> Ichor::ILifecycleManager::waitForService(DependencyManager &dm, uint64_t serviceId, uint64_t eventType) noexcept {
    return dm.waitForService(serviceId, eventType);
}