#pragma once

#include <ichor/Service.h>
#include <ichor/services/logging/Logger.h>

namespace Ichor {
    struct IStopsInAsyncStartService {
    };

    struct StopsInAsyncStartService final : public IStopsInAsyncStartService, public Service<StopsInAsyncStartService> {
        StopsInAsyncStartService() = default;
        ~StopsInAsyncStartService() final = default;

        AsyncGenerator<void> start() final {
            co_await getManager().pushPrioritisedEventAsync<StopServiceEvent>(getServiceId(), 50, false, getServiceId()).begin();
            getManager().pushEvent<QuitEvent>(getServiceId());

            co_return;
        }

        AsyncGenerator<void> stop() final {
            co_return;
        }
    };
}