#pragma once

#include <ichor/dependency_management/IService.h>
#include <ichor/stl/StrongTypedef.h>

namespace Ichor::v1 {
    struct ConnectionIdType : StrongTypedef<uint64_t, ConnectionIdType> {};

    struct ConnectionIdHash final {
        using is_transparent = void;
        using is_avalanching = void;

        auto operator()(ConnectionIdType const& x) const noexcept -> uint64_t {
            return ankerl::unordered_dense::detail::wyhash::hash(x.value);
        }
    };

    template <typename NetworkInterfaceType>
    class IClientFactory {
    public:
        /// Create a new connection which will be injected as a dependency. Connection will be automatically removed if requesting service is stopped, but removeConnection can be used as well.
        ///
        /// Common properties include Address, Port, NoDelay
        /// \param requestingSvc service requesting connection
        /// \param properties Properties relevant for the implementation
        /// \return id of new connection, to be used with removeConnection.
        virtual ConnectionIdType createNewConnection(NeverNull<IService*> requestingSvc, Properties properties) = 0;

        /// Removes connection
        /// \param requestingSvc service that originally requested connection
        /// \param connectionId Id returned from createNewConnection
        virtual void removeConnection(NeverNull<IService*> requestingSvc, ConnectionIdType connectionId) = 0;

    protected:
        ~IClientFactory() = default;
    };
}
