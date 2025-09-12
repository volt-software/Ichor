#pragma once
#include <type_traits>

namespace Ichor {
    template <typename T>
    struct AlwaysFalse_t : std::false_type {};

    template <typename T>
    constexpr bool AlwaysFalse_v = AlwaysFalse_t<T>::value;
}
