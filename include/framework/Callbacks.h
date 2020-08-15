#pragma once

#include <cstdint>
#include <functional>
#include "Generator.h"

namespace Cppelix {
    struct Event;

    class [[nodiscard]] EventCallbackInfo final {
    public:
        uint64_t listeningServiceId;
        std::optional<uint64_t> filterServiceId;
        std::function<Generator<bool>(Event const * const)> callback;
    };
}