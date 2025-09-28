#pragma once

#include <cstdint>
#include <string_view>
#include <fmt/format.h>

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
        Dependency() noexcept = default;
        explicit Dependency(uint64_t _interfaceNameHash, std::string_view _interfaceName, DependencyFlags _flags, uint64_t _satisfied) noexcept : interfaceNameHash(_interfaceNameHash), flags(_flags), satisfied(_satisfied), interfaceName(_interfaceName.data()), interfaceNameLength(_interfaceName.size()) {}
        Dependency(const Dependency &other) noexcept = default;
        Dependency(Dependency &&other) noexcept = default;
        Dependency& operator=(const Dependency &other) noexcept = default;
        Dependency& operator=(Dependency &&other) noexcept = default;
        bool operator==(const Dependency &other) const noexcept {
            return interfaceNameHash == other.interfaceNameHash && flags == other.flags;
        }
        [[nodiscard]] std::string_view getInterfaceName() const noexcept {
            return std::string_view{interfaceName, interfaceNameLength};
        }

        uint64_t interfaceNameHash;
        DependencyFlags flags;
        uint64_t satisfied;
    private:
        // this workaround is necessary to ensure trivially default constructability. But I would much rather it be possible to storage a string_view directly.
        char const *interfaceName;
        std::size_t interfaceNameLength;
    };

    static_assert(std::is_trivially_default_constructible_v<Dependency>, "Dependency is required to be trivially default constructible");
    static_assert(std::is_trivially_destructible_v<Dependency>, "Dependency is required to be trivially destructible");
}



template <>
struct fmt::formatter<Ichor::DependencyFlags> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::DependencyFlags& flags, FormatContext& ctx) const {
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
