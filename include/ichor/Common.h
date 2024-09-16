#pragma once

#include <ichor/ConstevalHash.h>
#include <ichor/stl/Any.h>
#include <ankerl/unordered_dense.h>
#include <string_view>

#if defined(ICHOR_ENABLE_INTERNAL_DEBUGGING) || defined(ICHOR_ENABLE_INTERNAL_COROUTINE_DEBUGGING) || defined(ICHOR_ENABLE_INTERNAL_IO_DEBUGGING) || defined(ICHOR_ENABLE_INTERNAL_STL_DEBUGGING)
#include <chrono>
#include <fmt/core.h>
#include <thread>
#include <fmt/std.h>
#include <ichor/stl/StringUtils.h>
#endif

#ifdef ICHOR_ENABLE_INTERNAL_DEBUGGING
static constexpr bool DO_INTERNAL_DEBUG = true;

#define INTERNAL_DEBUG(...)      \
    if constexpr(DO_INTERNAL_DEBUG) { \
        std::thread::id this_id = std::this_thread::get_id();                         \
        fmt::print("[{:L}] [{}] ", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), this_id);    \
        const char *base = Ichor::basename(__FILE__); \
        fmt::print("[{}:{}] ", base, __LINE__);    \
        fmt::println(__VA_ARGS__);    \
    }                                 \
static_assert(true, "")
#else
static constexpr bool DO_INTERNAL_DEBUG = false;

#define INTERNAL_DEBUG(...) static_assert(true, "")
#endif

#ifdef ICHOR_ENABLE_INTERNAL_COROUTINE_DEBUGGING
static constexpr bool DO_INTERNAL_COROUTINE_DEBUG = true;

#define INTERNAL_COROUTINE_DEBUG(...)      \
    if constexpr(DO_INTERNAL_COROUTINE_DEBUG) { \
        std::thread::id this_id = std::this_thread::get_id();                         \
        fmt::print("[{:L}] [{}] ", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), this_id);    \
        const char *base = Ichor::basename(__FILE__); \
        fmt::print("[{}:{}] ", base, __LINE__);    \
        fmt::println(__VA_ARGS__);    \
    }                                 \
static_assert(true, "")
#else
static constexpr bool DO_INTERNAL_COROUTINE_DEBUG = false;

#define INTERNAL_COROUTINE_DEBUG(...) static_assert(true, "")
#endif

#ifdef ICHOR_ENABLE_INTERNAL_IO_DEBUGGING
static constexpr bool DO_INTERNAL_IO_DEBUG = true;

#define INTERNAL_IO_DEBUG(...)      \
    if constexpr(DO_INTERNAL_IO_DEBUG) { \
        std::thread::id this_id = std::this_thread::get_id();                         \
        fmt::print("[{:L}] [{}] ", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), this_id);    \
        const char *base = Ichor::basename(__FILE__); \
        fmt::print("[{}:{}] ", base, __LINE__);    \
        fmt::println(__VA_ARGS__);    \
    }                                 \
static_assert(true, "")
#else
static constexpr bool DO_INTERNAL_IO_DEBUG = false;

#define INTERNAL_IO_DEBUG(...) static_assert(true, "")
#endif

#ifdef ICHOR_ENABLE_INTERNAL_STL_DEBUGGING
static constexpr bool DO_INTERNAL_STL_DEBUG = true;

#define INTERNAL_STL_DEBUG(...)      \
    if constexpr(DO_INTERNAL_STL_DEBUG) { \
        std::thread::id this_id = std::this_thread::get_id();                         \
        fmt::print("[{:L}] [{}] ", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(), this_id);    \
        const char *base = Ichor::basename(__FILE__); \
        fmt::print("[{}:{}] ", base, __LINE__);    \
        fmt::println(__VA_ARGS__);    \
    }                                 \
static_assert(true, "")
#else
static constexpr bool DO_INTERNAL_STL_DEBUG = false;

#define INTERNAL_STL_DEBUG(...) static_assert(true, "")
#endif

#ifdef ICHOR_USE_HARDENING
static constexpr bool DO_HARDENING = true;
#else
static constexpr bool DO_HARDENING = false;
#endif

#if defined(__has_cpp_attribute) && __has_cpp_attribute(assume)
#define ICHOR_ASSUME(...) [[assume(__VA_ARGS__)]];
#else
#define ICHOR_ASSUME(...) static_assert(true, "")
#endif

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

        uint64_t operator()(std::string_view txt) const   { return hash_type{}(txt); }
        uint64_t operator()(const std::string& txt) const { return hash_type{}(txt); }
        uint64_t operator()(const char* txt) const        { return hash_type{}(txt); }
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

    using Properties = unordered_map<std::string, Ichor::any, string_hash, std::equal_to<>>;

    inline constexpr bool PreventOthersHandling = false;
    inline constexpr bool AllowOthersHandling = true;

    // Code from https://artificial-mind.net/blog/2020/10/31/constexpr-for
    template <auto Start, auto End, auto Inc, class F>
    constexpr void constexpr_for(F&& f) {
        if constexpr (Start < End) {
            f(std::integral_constant<decltype(Start), Start>());
            constexpr_for<Start + Inc, End, Inc>(f);
        }
    }

    using ServiceIdType = uint64_t;
}
