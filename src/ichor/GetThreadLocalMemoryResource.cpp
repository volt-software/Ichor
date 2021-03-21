#include <ichor/GetThreadLocalMemoryResource.h>

thread_local std::pmr::memory_resource *rsrc;

std::pmr::memory_resource* Ichor::getThreadLocalMemoryResource() noexcept {
#ifndef NDEBUG
    if(rsrc == nullptr) {
        std::terminate();
    }
#endif
    return rsrc;
}

void Ichor::setThreadLocalMemoryResource(std::pmr::memory_resource* _rsrc) noexcept {
    rsrc = _rsrc;
}