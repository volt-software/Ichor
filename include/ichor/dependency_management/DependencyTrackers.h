#pragma once

namespace Ichor {
    struct DependencyTrackerInfo final {
        explicit DependencyTrackerInfo(ServiceIdType _svcId, std::function<void(Event const &)> _trackFunc, std::function<void(Event const &)> _untrackFunc) noexcept : svcId(_svcId), trackFunc(std::move(_trackFunc)), untrackFunc(std::move(_untrackFunc)) {}
        ServiceIdType svcId;
        std::function<void(Event const &)> trackFunc;
        std::function<void(Event const &)> untrackFunc;
    };

    struct DependencyTrackerKey final {
        DependencyTrackerKey(std::string_view name, NameHashType nameHash) : interfaceName(name), interfaceNameHash(nameHash) {}

        bool operator==(DependencyTrackerKey const &dtk) const noexcept {
            return dtk.interfaceNameHash == interfaceNameHash;
        }

        bool operator==(NameHashType hash) const noexcept {
            return hash == interfaceNameHash;
        }

        std::string_view interfaceName;
        NameHashType interfaceNameHash;
    };

    struct DependencyTrackerKeyHash final {
        using is_transparent = void;
        using is_avalanching = void;

        auto operator()(DependencyTrackerKey const& x) const noexcept -> uint64_t {
            return ankerl::unordered_dense::detail::wyhash::hash(x.interfaceNameHash);
        }

        auto operator()(NameHashType hash) const noexcept -> uint64_t {
            return ankerl::unordered_dense::detail::wyhash::hash(hash);
        }
    };
}
