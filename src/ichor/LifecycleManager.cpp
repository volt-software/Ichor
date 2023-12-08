#include <ichor/DependencyManager.h>

Ichor::Task<void> Ichor::ILifecycleManager::waitForService(uint64_t serviceId, uint64_t eventType) noexcept {
    return GetThreadLocalManager().waitForService(serviceId, eventType);
}
