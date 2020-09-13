#pragma once

#include <cppelix/Service.h>

namespace Cppelix {
    class IHostService : public virtual IService {
    public:
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};

        ~IHostService() override = default;

        virtual void set_priority(uint64_t priority) = 0;
        virtual uint64_t get_priority() = 0;
    };
}