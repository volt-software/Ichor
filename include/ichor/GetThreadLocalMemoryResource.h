#pragma once

#include <memory_resource>

namespace Ichor {
    std::pmr::memory_resource* getThreadLocalMemoryResource() noexcept;
    void setThreadLocalMemoryResource(std::pmr::memory_resource* _rsrc) noexcept;
}