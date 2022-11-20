#pragma once

#include <cstdint>
#include <optional>
#include <functional>
#include <ichor/Enums.h>

namespace Ichor {
    struct Event;
    template <typename T>
    class AsyncGenerator;

    class [[nodiscard]] EventCallbackInfo final {
    public:
        uint64_t listeningServiceId;
        std::optional<uint64_t> filterServiceId;
        std::function<AsyncGenerator<IchorBehaviour>(Event const &)> callback;
    };

    class [[nodiscard]] EventInterceptInfo final {
    public:
        uint64_t listeningServiceId;
        std::optional<uint64_t> filterEventId;
        std::function<bool(Event const &)> preIntercept;
        std::function<void(Event const &, bool)> postIntercept;
    };

    struct CallbackKey {
        uint64_t id;
        uint64_t type;

        bool operator==(const CallbackKey &other) const noexcept {
            return id == other.id && type == other.type;
        }

#ifdef ICHOR_USE_ABSEIL
        template <typename H>
        friend H AbslHashValue(H h, const CallbackKey& c) {
            return H::combine(std::move(h), c.id, c.type);
        }
#endif
    };
}

namespace std {
    template <>
    struct hash<Ichor::CallbackKey> {
        std::size_t operator()(const Ichor::CallbackKey& k) const noexcept {
            return k.id ^ k.type;
        }
    };
}