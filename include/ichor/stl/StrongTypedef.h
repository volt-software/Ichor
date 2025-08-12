#pragma once

namespace Ichor {
    template <typename ValueType, typename Tag>
    struct StrongTypedef {
        auto operator<=>(const StrongTypedef&) const = default;

        StrongTypedef& operator++() {
            ++value;
            return *this;
        }

        StrongTypedef operator++(int) {
            ValueType old = value;
            ++value;
            return StrongTypedef{old};
        }

        StrongTypedef& operator--() {
            --value;
            return *this;
        }

        StrongTypedef operator--(int) {
            ValueType old = value;
            --value;
            return StrongTypedef{old};
        }

        ValueType value;
    };
}
