#pragma once

#include <framework/Service.h>

namespace Cppelix {
    class IConnectionService : public virtual IService {
    public:
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};

        ~IConnectionService() override = default;

        virtual void send(std::vector<uint8_t>&& msg) = 0;

        virtual void set_priority(uint64_t priority) = 0;
        virtual uint64_t get_priority() = 0;
    };
}