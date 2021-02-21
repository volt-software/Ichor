
#include <ichor/Service.h>
#include <ichor/DependencyManager.h>

std::atomic<uint64_t> Ichor::_serviceIdCounter = 1;

std::pmr::memory_resource* Ichor::IService::getMemoryResource() noexcept {
    return getManager()->getMemoryResource();
}