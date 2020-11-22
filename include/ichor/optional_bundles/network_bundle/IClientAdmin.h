#pragma once

#include <ichor/Service.h>

namespace Ichor {
    class IClientAdmin : public virtual IService {
    public:
        ~IClientAdmin() override = default;
    };
}