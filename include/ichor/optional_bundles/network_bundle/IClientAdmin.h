#pragma once

#include <ichor/Service.h>

namespace Ichor {
    class IClientAdmin : virtual public IService {
    public:
        ~IClientAdmin() override = default;
    };
}