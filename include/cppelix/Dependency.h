#pragma once

#include <cppelix/Common.h>

namespace Cppelix {
    struct Dependency {
        CPPELIX_CONSTEXPR Dependency(uint64_t _interfaceNameHash, InterfaceVersion _interfaceVersion, bool _required) noexcept : interfaceNameHash(_interfaceNameHash), interfaceVersion(_interfaceVersion), required(_required) {}
        CPPELIX_CONSTEXPR Dependency(const Dependency &other) noexcept = default;
        CPPELIX_CONSTEXPR Dependency(Dependency &&other) noexcept = default;
        CPPELIX_CONSTEXPR Dependency& operator=(const Dependency &other) noexcept = default;
        CPPELIX_CONSTEXPR Dependency& operator=(Dependency &&other) noexcept = default;
        CPPELIX_CONSTEXPR bool operator==(const Dependency &other) const noexcept {
            return interfaceNameHash == other.interfaceNameHash && interfaceVersion == other.interfaceVersion && required == other.required;
        }

        uint64_t interfaceNameHash;
        InterfaceVersion interfaceVersion;
        bool required;
    };
}