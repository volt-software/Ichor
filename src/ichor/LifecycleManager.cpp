#include <ichor/LifecycleManager.h>
#include <ichor/DependencyManager.h>

Ichor::DependencyRegister::DependencyRegister(DependencyManager *mng) noexcept : _registrations(mng->getMemoryResource()) {

}