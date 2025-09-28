#pragma once

#include <ichor/ConstevalHash.h>
#include <ichor/stl/Any.h>
#include <ankerl/unordered_dense.h>
#include <string_view>

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

    using Properties = unordered_map<std::string, Ichor::v1::any, string_hash, std::equal_to<>>;

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
}
