#pragma once

#include <cstdint>

struct ICountService {
    [[nodiscard]] virtual uint64_t getSvcCount() const noexcept = 0;
    [[nodiscard]] virtual bool isRunning() const noexcept = 0;
protected:
    ~ICountService() = default;
};
