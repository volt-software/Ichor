
#include <ichor/Service.h>
#include <ichor/DependencyManager.h>

std::atomic<uint64_t> Ichor::_serviceIdCounter = 1;

#ifdef ICHOR_ENABLE_MEMORY_CONTROL
std::memory_resource* Ichor::IService::getMemoryResource() noexcept {
    return getManager()->getMemoryResource();
}
#endif