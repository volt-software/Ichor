#include <ichor/DependencyManager.h>

ICHOR_CORO_WRAPPER Ichor::Task<void> Ichor::ILifecycleManager::waitForService(ServiceIdType serviceId, uint64_t eventType) noexcept {
    return GetThreadLocalManager().waitForService(serviceId, eventType);
}
