#include <ichor/LifecycleManager.h>
#include <ichor/DependencyManager.h>

#ifndef ICHOR_ENABLE_MEMORY_CONTROL
Ichor::DependencyRegister::DependencyRegister(DependencyManager *mng) noexcept : _registrations() {
#else
Ichor::DependencyRegister::DependencyRegister(DependencyManager *mng) noexcept : _registrations(mng->getMemoryResource()) {
#endif

}