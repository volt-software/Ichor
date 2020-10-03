#pragma once

#include <fmt/format.h>
#include <ichor/Common.h>
#include <ichor/Service.h>

namespace Ichor {
    class IShellCommand : virtual public IService {
    public:
        
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
    protected:
        virtual ~IShellCommand() = default;
    };
}