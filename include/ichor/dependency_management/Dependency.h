#pragma once

#include <ichor/Common.h>

namespace Ichor {

    enum class DependencyFlags : uint_fast16_t {
        NONE = 0,
        REQUIRED = 1,
        ALLOW_MULTIPLE = 2
    };

    static constexpr inline DependencyFlags operator|(DependencyFlags lhs, DependencyFlags rhs) noexcept {
        return static_cast<DependencyFlags>(static_cast<uint_fast16_t>(lhs) | static_cast<uint_fast16_t>(rhs));
    }

    static constexpr inline DependencyFlags operator&(DependencyFlags lhs, DependencyFlags rhs) noexcept {
        return static_cast<DependencyFlags>(static_cast<uint_fast16_t>(lhs) & static_cast<uint_fast16_t>(rhs));
    }

    struct Dependency {
        Dependency(uint64_t _interfaceNameHash, std::string_view _interfaceName, DependencyFlags _flags, uint64_t _satisfied) noexcept : interfaceNameHash(_interfaceNameHash), interfaceName(_interfaceName), flags(_flags), satisfied(_satisfied) {}
        Dependency(const Dependency &other) noexcept = default;
        Dependency(Dependency &&other) noexcept = default;
        Dependency& operator=(const Dependency &other) noexcept = default;
        Dependency& operator=(Dependency &&other) noexcept = default;
        bool operator==(const Dependency &other) const noexcept {
            return interfaceNameHash == other.interfaceNameHash && flags == other.flags;
        }

        uint64_t interfaceNameHash;
        std::string_view interfaceName;
        DependencyFlags flags;
        uint64_t satisfied;
    };
}



template <>
struct fmt::formatter<Ichor::DependencyFlags> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::DependencyFlags& flags, FormatContext& ctx) {
        if(flags == Ichor::DependencyFlags::NONE) {
            return fmt::format_to(ctx.out(), "NONE");
        }

        std::string intermediate{};
        if((flags & Ichor::DependencyFlags::REQUIRED) == Ichor::DependencyFlags::REQUIRED) {
            fmt::format_to(std::back_inserter(intermediate), "REQUIRED");
        }
        if((flags & Ichor::DependencyFlags::ALLOW_MULTIPLE) == Ichor::DependencyFlags::ALLOW_MULTIPLE) {
            if(intermediate.empty()) {
                fmt::format_to(std::back_inserter(intermediate), "ALLOW_MULTIPLE");
            } else {
                fmt::format_to(std::back_inserter(intermediate), "|ALLOW_MULTIPLE");
            }
        }

        if(intermediate.empty()) {
            return fmt::format_to(ctx.out(), "Unknown value. Please file a bug in Ichor.");;
        }

        return fmt::format_to(ctx.out(), "{}", intermediate);
    }
};
