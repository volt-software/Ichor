#pragma once

#include <cstdint>
#include <functional>
#include <cppelix/Generator.h>

namespace Cppelix {
    struct Event;

    class [[nodiscard]] EventCallbackInfo final {
    public:
        uint64_t listeningServiceId;
        std::optional<uint64_t> filterServiceId;
        std::function<Generator<bool>(Event const * const)> callback;
    };

    class [[nodiscard]] EventInterceptInfo final {
    public:
        uint64_t listeningServiceId;
        std::optional<uint64_t> filterEventId;
        std::function<bool(Event const * const)> preIntercept;
        std::function<bool(Event const * const, bool)> postIntercept;
    };
}