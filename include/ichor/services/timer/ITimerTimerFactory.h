#pragma once

#include <vector>
#include <ichor/Common.h>

namespace Ichor {
    struct ITimerTimerFactory {
        [[nodiscard]] virtual std::vector<ServiceIdType> getCreatedTimerFactoryIds() const noexcept = 0;

    protected:
        ~ITimerTimerFactory() = default;
    };
}
