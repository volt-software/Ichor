#include <ichor/DependencyManager.h>

ICHOR_CORO_WRAPPER Ichor::Task<void> Ichor::ILifecycleManager::waitForService(uint64_t serviceId, uint64_t eventType) noexcept {
    return GetThreadLocalManager().waitForService(serviceId, eventType);
}
