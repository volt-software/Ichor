#pragma once

#include <ichor/coroutines/Task.h>
#include <ichor/dependency_management/IService.h>

namespace Ichor {
    class IClientFactory {
    public:
        /// Create a new connection which will be injected as a dependency. Connection will be automatically removed if requesting service is stopped, but removeConnection can be used as well.
        ///
        /// Common properties include Address, Port, NoDelay
        /// \param requestingSvc service requesting connection
        /// \param properties Properties relevant for the implementation
        /// \return id of new connection, to be used with removeConnection.
        virtual ServiceIdType createNewConnection(NeverNull<IService*> requestingSvc, Properties properties) = 0;

        /// Removes connection
        /// \param requestingSvc service that originally requested connection
        /// \param connectionId Id returned from createNewConnection
        virtual void removeConnection(NeverNull<IService*> requestingSvc, ServiceIdType connectionId) = 0;

    protected:
        ~IClientFactory() = default;
    };
}
