#pragma once

#include <ichor/Service.h>

namespace Ichor {
    class IHostService : virtual public IService {
    public:
        ~IHostService() override = default;

        virtual void setPriority(uint64_t priority) = 0;
        virtual uint64_t getPriority() = 0;
    };
}