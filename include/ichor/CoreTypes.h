#pragma once

#include <ichor/stl/StrongTypedef.h>
#include <ankerl/unordered_dense.h>
#include <fmt/format.h>

namespace Ichor {
    struct ServiceIdType : StrongTypedef<uint64_t, ServiceIdType> {};

    struct ServiceIdHash final {
        using is_transparent = void;
        using is_avalanching = void;

        auto operator()(ServiceIdType const& x) const noexcept -> uint64_t {
            return ankerl::unordered_dense::detail::wyhash::hash(x.value);
        }
    };
}



template <>
struct fmt::formatter<Ichor::ServiceIdType> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::ServiceIdType& id, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", id.value);
    }
};