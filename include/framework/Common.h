#pragma once

#include "framework/ConstevalHash.h"
#include <any>
#include <string_view>
#include <unordered_map>

#if __cpp_lib_constexpr_string >= 201907L
#if __cpp_lib_constexpr_vector >= 201907L
#define CPPELIX_CONSTEXPR constexpr
#else
#define CPPELIX_CONSTEXPR
#endif
#else
#define CPPELIX_CONSTEXPR
#endif

// GNU C Library contains defines in sys/sysmacros.h. However, for compatibility reasons, this header is included in sys/types.h. Which is used by std.
#undef major
#undef minor

namespace Cppelix {
    template<typename INTERFACE_TYPENAME>
    [[nodiscard]] consteval auto typeName() {
        constexpr std::string_view result = __PRETTY_FUNCTION__;
        constexpr std::string_view templateStr = "INTERFACE_TYPENAME = ";

        constexpr size_t bpos = result.find(templateStr) + templateStr.size(); //find begin pos after INTERFACE_TYPENAME = entry
        if constexpr (result.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_:") == std::string_view::npos) {
            constexpr size_t len = result.length() - bpos;

            static_assert(!result.substr(bpos, len).empty(), "Cannot infer type name in function call");

            return result.substr(bpos, len);
        } else {
            constexpr size_t len = result.substr(bpos).find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_:");

            static_assert(!result.substr(bpos, len).empty(), "Cannot infer type name in function call");

            return result.substr(bpos, len);
        }
    }

    template<typename INTERFACE_TYPENAME>
    [[nodiscard]] consteval auto typeNameHash() {
        std::string_view name = typeName<INTERFACE_TYPENAME>();
        return consteval_wyhash(&name[0], name.size(), 0);
    }

    struct InterfaceVersion {
        constexpr InterfaceVersion(uint64_t _major, uint64_t _minor, uint64_t _patch) noexcept : major(_major), minor(_minor), patch(_patch) {}
        constexpr InterfaceVersion(const InterfaceVersion &other) noexcept = default;
        constexpr InterfaceVersion(InterfaceVersion &&other) noexcept = default;
        constexpr InterfaceVersion& operator=(const InterfaceVersion &other) noexcept = default;
        constexpr InterfaceVersion& operator=(InterfaceVersion &&other) noexcept = default;
        constexpr std::strong_ordering operator<=>(const InterfaceVersion&) const noexcept = default;

        uint64_t major;
        uint64_t minor;
        uint64_t patch;
    };

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

#if __cpp_lib_generic_unordered_lookup >= 201811
    struct string_hash {
        using transparent_key_equal = std::equal_to<>;  // Pred to use
        using hash_type = std::hash<std::string_view>;  // just a helper local type
        size_t operator()(std::string_view txt) const   { return hash_type{}(txt); }
        size_t operator()(const std::string& txt) const { return hash_type{}(txt); }
        size_t operator()(const char* txt) const        { return hash_type{}(txt); }
    };

    using CppelixProperties = std::unordered_map<std::string, std::any, string_hash>;
#else
    using CppelixProperties = std::unordered_map<std::string, std::any>;
#endif

    inline constexpr bool PreventOthersHandling = false;
    inline constexpr bool AllowOthersHandling = false;
}