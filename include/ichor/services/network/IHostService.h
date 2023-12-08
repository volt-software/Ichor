#pragma once

#include <cstdint>

namespace Ichor {
    class IHostService {
    public:
        virtual void setPriority(uint64_t priority) = 0;
        virtual uint64_t getPriority() = 0;

    protected:
        ~IHostService() = default;
    };
}
