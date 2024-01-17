#pragma once

#include <ichor/ConstevalHash.h>
#include <ichor/stl/Any.h>
#include <string_view>
#include <chrono>
#include <fmt/core.h>

#include <ankerl/unordered_dense.h>

#ifdef ICHOR_ENABLE_INTERNAL_DEBUGGING
static constexpr bool DO_INTERNAL_DEBUG = true;
#else
static constexpr bool DO_INTERNAL_DEBUG = false;
#endif

#ifdef ICHOR_ENABLE_INTERNAL_COROUTINE_DEBUGGING
static constexpr bool DO_INTERNAL_COROUTINE_DEBUG = true;
#else
static constexpr bool DO_INTERNAL_COROUTINE_DEBUG = false;
#endif

#ifdef ICHOR_ENABLE_INTERNAL_IO_DEBUGGING
static constexpr bool DO_INTERNAL_IO_DEBUG = true;
#else
static constexpr bool DO_INTERNAL_IO_DEBUG = false;
#endif

#ifdef ICHOR_ENABLE_INTERNAL_STL_DEBUGGING
static constexpr bool DO_INTERNAL_STL_DEBUG = true;
#else
static constexpr bool DO_INTERNAL_STL_DEBUG = false;
#endif

#ifdef ICHOR_USE_HARDENING
static constexpr bool DO_HARDENING = true;
#else
static constexpr bool DO_HARDENING = false;
#endif

#define INTERNAL_DEBUG(...)      \
    if constexpr(DO_INTERNAL_DEBUG) { \
        fmt::print("[{:L}] ", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());    \
        fmt::print("[{}:{}] ", __FILE__, __LINE__);    \
        fmt::print(__VA_ARGS__);    \
        fmt::print("\n");    \
    }                                 \
static_assert(true, "")

#define INTERNAL_COROUTINE_DEBUG(...)      \
    if constexpr(DO_INTERNAL_COROUTINE_DEBUG) { \
        fmt::print("[{:L}] ", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());    \
        fmt::print("[{}:{}] ", __FILE__, __LINE__);    \
        fmt::print(__VA_ARGS__);    \
        fmt::print("\n");    \
    }                                 \
static_assert(true, "")

#define INTERNAL_IO_DEBUG(...)      \
    if constexpr(DO_INTERNAL_IO_DEBUG) { \
        fmt::print("[{:L}] ", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());    \
        fmt::print("[{}:{}] ", __FILE__, __LINE__);    \
        fmt::print(__VA_ARGS__);    \
        fmt::print("\n");    \
    }                                 \
static_assert(true, "")

#define INTERNAL_STL_DEBUG(...)      \
    if constexpr(DO_INTERNAL_STL_DEBUG) { \
        fmt::print("[{:L}] ", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());    \
        fmt::print("[{}:{}] ", __FILE__, __LINE__);    \
        fmt::print(__VA_ARGS__);    \
        fmt::print("\n");    \
    }                                 \
static_assert(true, "")

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
        using is_avalanching = void;
        using is_transparent = void;  // Pred to use
        using key_equal = std::equal_to<>;  // Pred to use
        using hash_type = ankerl::unordered_dense::hash<std::string_view>;  // just a helper local type

        size_t operator()(std::string_view txt) const   { return hash_type{}(txt); }
        size_t operator()(const std::string& txt) const { return hash_type{}(txt); }
        size_t operator()(const char* txt) const        { return hash_type{}(txt); }
    };

    template <
            class Key,
            class T,
            class Hash = ankerl::unordered_dense::hash<Key>,
            class KeyEqual = std::equal_to<Key>,
            class Allocator = std::allocator<std::pair<Key, T>>>
    using unordered_map = ankerl::unordered_dense::map<Key, T, Hash, KeyEqual, Allocator>;

    template <
            class T,
            class Hash = ankerl::unordered_dense::hash<T>,
            class Eq = std::equal_to<T>,
            class Allocator = std::allocator<T>>
    using unordered_set = ankerl::unordered_dense::set<T, Hash, Eq, Allocator>;

    using Properties = unordered_map<std::string, Ichor::any, string_hash>;

    inline constexpr bool PreventOthersHandling = false;
    inline constexpr bool AllowOthersHandling = true;

    /// Code modified from https://stackoverflow.com/a/73078442/1460998
    /// converts a string to an integer with little error checking. Only use if you're very sure that the string is actually a number.
    static inline int64_t FastAtoiCompare(const char* str) noexcept {
        int64_t val = 0;
        uint8_t x;
        bool neg{};
        if(str[0] == '-') {
            str++;
            neg = true;
        }
        while ((x = uint8_t(*str++ - '0')) <= 9) val = val * 10 + x;
        return neg ? -val : val;
    }

    /// Code from https://stackoverflow.com/a/73078442/1460998
    /// converts a string to an unsigned integer with little error checking. Only use if you're very sure that the string is actually a number.
    static inline uint64_t FastAtoiCompareu(const char* str) noexcept {
        uint64_t val = 0;
        uint8_t  x;
        while ((x = uint8_t(*str++ - '0')) <= 9) val = val * 10 + x;
        return val;
    }

    // Code from https://artificial-mind.net/blog/2020/10/31/constexpr-for
    template <auto Start, auto End, auto Inc, class F>
    constexpr void constexpr_for(F&& f) {
        if constexpr (Start < End) {
            f(std::integral_constant<decltype(Start), Start>());
            constexpr_for<Start + Inc, End, Inc>(f);
        }
    }
}
