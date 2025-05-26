#pragma once

#include <ichor/services/network/IHostService.h>

struct HostServiceMock : public IHostService, public AdvancedService<HostServiceMock> {
    HostServiceMock(DependencyRegister &reg, Properties props) : AdvancedService<HostServiceMock>(std::move(props)) {}

    void setPriority(uint64_t _priority) final {
        priority = _priority;
    }

    uint64_t getPriority() final {
        return priority;
    }

    uint64_t priority{};
};
