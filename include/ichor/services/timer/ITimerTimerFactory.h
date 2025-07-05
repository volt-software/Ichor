#pragma once

#include <vector>

namespace Ichor::v1 {
    struct ITimerTimerFactory {
        [[nodiscard]] virtual std::vector<ServiceIdType> getCreatedTimerFactoryIds() const noexcept = 0;

    protected:
        ~ITimerTimerFactory() = default;
    };
}
