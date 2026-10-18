#pragma once

#include <compare>

namespace Ichor {
    template <typename ValueType, typename Tag>
    // NOLINTNEXTLINE(bugprone-crtp-constructor-accessibility): an aggregate, so that Tag{value} works, which a private constructor would prevent
    struct StrongTypedef {
        auto operator<=>(const StrongTypedef&) const = default;

        Tag& operator++() {
            ++value;
            return static_cast<Tag&>(*this);
        }

        Tag operator++(int) {
            Tag old = Tag{value};
            ++value;
            return old;
        }

        Tag& operator--() {
            --value;
            return static_cast<Tag&>(*this);
        }

        Tag operator--(int) {
            Tag old = Tag{value};
            --value;
            return old;
        }

        ValueType value;
    };
}
