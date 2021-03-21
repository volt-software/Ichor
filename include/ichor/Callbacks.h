#pragma once

#include <cstdint>
#include <ichor/Generator.h>
#include <ichor/stl/Function.h>

namespace Ichor {
    struct Event;

    class [[nodiscard]] EventCallbackInfo final {
    public:
        uint64_t listeningServiceId;
        std::optional<uint64_t> filterServiceId;
        Ichor::function<Generator<bool>(Event const * const)> callback;
    };

    class [[nodiscard]] EventInterceptInfo final {
    public:
        uint64_t listeningServiceId;
        std::optional<uint64_t> filterEventId;
        Ichor::function<bool(Event const * const)> preIntercept;
        Ichor::function<bool(Event const * const, bool)> postIntercept;
    };

    struct CallbackKey {
        uint64_t id;
        uint64_t type;

        bool operator==(const CallbackKey &other) const {
            return id == other.id && type == other.type;
        }
    };
}

namespace std {
    template <>
    struct hash<Ichor::CallbackKey> {
        std::size_t operator()(const Ichor::CallbackKey& k) const {
            return k.id ^ k.type;
        }
    };
}