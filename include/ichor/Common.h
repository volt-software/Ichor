#pragma once

#include <ichor/ConstevalHash.h>
#include <ichor/stl/Any.h>
#include <string_view>
#include <unordered_map>

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#if !__has_include(<spdlog/spdlog.h>)
#define SPDLOG_DEBUG(...)
#else
#include <spdlog/spdlog.h>
#endif

// note that this only works when compiling with spdlog
static constexpr bool DO_INTERNAL_DEBUG = false;

#define INTERNAL_DEBUG(...) do {      \
    if constexpr(DO_INTERNAL_DEBUG) { \
        SPDLOG_DEBUG(__VA_ARGS__);    \
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
        using hash_type = std::hash<std::string_view>;  // just a helper local type
        size_t operator()(std::string_view txt) const   { return hash_type{}(txt); }
        size_t operator()(const std::string& txt) const { return hash_type{}(txt); }
        size_t operator()(const char* txt) const        { return hash_type{}(txt); }
    };

    using Properties = std::unordered_map<std::string, Ichor::any, string_hash>;
    using IchorProperty = std::pair<std::string, Ichor::any>;

    inline constexpr bool PreventOthersHandling = false;
    inline constexpr bool AllowOthersHandling = true;

    template <class T>
    concept Pair = std::is_same_v<T, IchorProperty>;

    template <Pair... Pairs>
    Properties make_properties( Pairs&&... pairs) {
        Properties props{};
        props.reserve(sizeof...(Pairs));
        (props.emplace(std::move(pairs.first), std::move(pairs.second)), ...);
        return props;
    }
}