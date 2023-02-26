#include <ichor/DependencyManager.h>

Ichor::AsyncGenerator<void> Ichor::ILifecycleManager::waitForService(uint64_t serviceId, uint64_t eventType) noexcept {
    return GetThreadLocalManager().waitForService(serviceId, eventType);
}
