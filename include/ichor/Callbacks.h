#pragma once

#include <cstdint>
#include <tl/optional.h>
#include <functional>
#include <ichor/Enums.h>
#include <ichor/stl/CompilerSpecific.h>

namespace Ichor {
    struct Event;
    template <typename T>
    class ICHOR_CORO_AWAIT_ELIDABLE ICHOR_CORO_LIFETIME_BOUND AsyncGenerator;

    class [[nodiscard]] EventCallbackInfo final {
    public:
        uint64_t listeningServiceId;
        tl::optional<uint64_t> filterServiceId;
        std::function<AsyncGenerator<IchorBehaviour>(Event const &)> callback;
    };

    class [[nodiscard]] EventInterceptInfo final {
    public:
        uint64_t interceptorId;
        uint64_t listeningServiceId;
        std::function<bool(Event const &)> preIntercept;
        std::function<void(Event const &, bool)> postIntercept;
    };

    struct CallbackKey {
        uint64_t id;
        uint64_t type;

        bool operator==(const CallbackKey &other) const noexcept {
            return id == other.id && type == other.type;
        }
    };
}

namespace std {
    template <>
    struct hash<Ichor::CallbackKey> {
        uint64_t operator()(const Ichor::CallbackKey& k) const noexcept {
            return k.id ^ k.type;
        }
    };
}
