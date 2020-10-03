#pragma once

#include <ichor/Common.h>

namespace Ichor {
    struct Dependency {
        ICHOR_CONSTEXPR Dependency(uint64_t _interfaceNameHash, InterfaceVersion _interfaceVersion, bool _required) noexcept : interfaceNameHash(_interfaceNameHash), interfaceVersion(_interfaceVersion), required(_required) {}
        ICHOR_CONSTEXPR Dependency(const Dependency &other) noexcept = default;
        ICHOR_CONSTEXPR Dependency(Dependency &&other) noexcept = default;
        ICHOR_CONSTEXPR Dependency& operator=(const Dependency &other) noexcept = default;
        ICHOR_CONSTEXPR Dependency& operator=(Dependency &&other) noexcept = default;
        ICHOR_CONSTEXPR bool operator==(const Dependency &other) const noexcept {
            return interfaceNameHash == other.interfaceNameHash && interfaceVersion == other.interfaceVersion && required == other.required;
        }

        uint64_t interfaceNameHash;
        InterfaceVersion interfaceVersion;
        bool required;
    };
}