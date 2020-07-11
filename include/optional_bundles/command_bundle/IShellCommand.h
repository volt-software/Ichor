#pragma once

#include <fmt/format.h>
#include "framework/Common.h"
#include "framework/Service.h"

namespace Cppelix {
    class IShellCommand : virtual public IService {
    public:
        
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
    protected:
        virtual ~IShellCommand() = default;
    };
}