#include <ichor/DependencyManager.h>

ICHOR_CORO_WRAPPER Ichor::Task<void> Ichor::ILifecycleManager::waitForService(ServiceIdType serviceId, uint64_t eventType) noexcept {
    return GetThreadLocalManager().waitForService(serviceId, eventType);
}

bool Ichor::ILifecycleManager::hasDependencyWaiter(ServiceIdType serviceId, uint64_t eventType) noexcept {
    return GetThreadLocalManager().hasDependencyWaiter(serviceId, eventType);
}