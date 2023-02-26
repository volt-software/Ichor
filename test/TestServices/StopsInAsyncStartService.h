#pragma once

#include <ichor/dependency_management/AdvancedService.h>

namespace Ichor {
    struct IStopsInAsyncStartService {
    };

    struct StopsInAsyncStartService final : public IStopsInAsyncStartService, public AdvancedService<StopsInAsyncStartService> {
        StopsInAsyncStartService() = default;
        ~StopsInAsyncStartService() final = default;

        Task<tl::expected<void, Ichor::StartError>> start() final {
            co_await GetThreadLocalManager().pushPrioritisedEventAsync<StopServiceEvent>(getServiceId(), 50, false, getServiceId()).begin();
            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());

            co_return {};
        }

        Task<void> stop() final {
            co_return;
        }
    };
}
