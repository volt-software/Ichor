#pragma once

#include <ichor/ConstevalHash.h>
#include <ichor/stl/Any.h>
#include <string_view>

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#if !__has_include(<spdlog/spdlog.h>)
#define SPDLOG_DEBUG(...)
#else
#include <spdlog/spdlog.h>
#endif


#ifdef ICHOR_USE_ABSEIL
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/hash/hash.h>
#else
#include <unordered_map>
#include <unordered_set>
#endif

#ifdef ICHOR_ENABLE_INTERNAL_DEBUGGING
static constexpr bool DO_INTERNAL_DEBUG = true;
#else
static constexpr bool DO_INTERNAL_DEBUG = false;
#endif

#ifdef ICHOR_USE_HARDENING
static constexpr bool DO_HARDENING = true;
#else
static constexpr bool DO_HARDENING = false;
#endif

#define INTERNAL_DEBUG(...) do {      \
    if constexpr(DO_INTERNAL_DEBUG) { \
        fmt::print("[{}:{}] ", __FILE__, __LINE__);    \
        fmt::print(__VA_ARGS__);    \
        fmt::print("\n");    \
    }                                 \
} while (0)

// GNU C Library contains defines in sys/sysmacros.h. However, for compatibility reasons, this header is included in sys/types.h. Which is used by std.
#undef major
#undef minor

namespace Ichor {
    template<typename...>
    struct typeList {};

    template<typename... Type>
    struct OptionalList_t: typeList<Type...>{};

    template<typename... Type>
    struct RequiredList_t: typeList<Type...>{};

    template<typename... Type>
    struct InterfacesList_t: typeList<Type...>{};

    template<typename... Type>
    inline constexpr OptionalList_t<Type...> OptionalList{};

    template<typename... Type>
    inline constexpr RequiredList_t<Type...> RequiredList{};

    template<typename... Type>
    inline constexpr InterfacesList_t<Type...> InterfacesList{};


    struct string_hash {
        using is_transparent = void;  // Pred to use
        using key_equal = std::equal_to<>;  // Pred to use
#ifdef ICHOR_USE_ABSEIL
        using hash_type = absl::Hash<std::string_view>;  // just a helper local type
#else
        using hash_type = std::hash<std::string_view>;  // just a helper local type
#endif
        size_t operator()(std::string_view txt) const   { return hash_type{}(txt); }
        size_t operator()(const std::string& txt) const { return hash_type{}(txt); }
        size_t operator()(const char* txt) const        { return hash_type{}(txt); }
    };


#ifdef ICHOR_USE_ABSEIL
    // We use std::hash instead of absl::hash because a lot of Ichor uses various ints as keys
    // and absl::hash opts to hash those instead of using an identity function. This gives better
    // avalanching, at the cost of performance. Ichor doesn't need avalanching in its use-cases.
    template <
            class Key,
            class T,
            class Hash = std::hash<Key>,
            class KeyEqual = std::equal_to<Key>,
            class Allocator = std::allocator<std::pair<const Key, T>>>
    using unordered_map = absl::flat_hash_map<Key, T, Hash, KeyEqual, Allocator>;
    template <
            class T,
            class Hash = std::hash<T>,
            class Eq = std::equal_to<T>,
            class Allocator = std::allocator<T>>
    using unordered_set = absl::flat_hash_set<T, Hash, Eq, Allocator>;
    template <class T>
    using b = std::unordered_set<T>;
#else
    template <
            class Key,
            class T,
            class Hash = std::hash<Key>,
            class KeyEqual = std::equal_to<Key>,
            class Allocator = std::allocator<std::pair<const Key, T>>>
    using unordered_map = std::unordered_map<Key, T, Hash, KeyEqual, Allocator>;
    template <
            class T,
            class Hash = std::hash<T>,
            class Eq = std::equal_to<T>,
            class Allocator = std::allocator<T>>
    using unordered_set = std::unordered_set<T, Hash, Eq, Allocator>;
#endif

    using Properties = unordered_map<std::string, Ichor::any, string_hash>;

    inline constexpr bool PreventOthersHandling = false;
    inline constexpr bool AllowOthersHandling = true;
}