#pragma once

#include <cstdint>
#include <cppcoro/generator.hpp>
#include <functional>

namespace Cppelix {
    class Event;

    class EventCallbackReturn final {
    public:
        bool continuable;
        bool allowOtherHandlers;
    };

    class [[nodiscard]] EventCallbackInfo final {
    public:
        uint64_t listeningServiceId;
        std::optional<uint64_t> filterServiceId;
        std::function<cppcoro::generator<EventCallbackReturn>(Event const * const)> callback;
    };
}