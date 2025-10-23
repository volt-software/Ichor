#pragma once

using namespace Ichor;
using namespace Ichor::v1;

extern std::unique_ptr<Ichor::AsyncManualResetEvent> _evt;

struct IAsyncDependencyTrackerService {
    [[nodiscard]] virtual unordered_map<ServiceIdType, ServiceIdType, ServiceIdHash> getCreatedServices() noexcept = 0;
protected:
    ~IAsyncDependencyTrackerService() = default;
};

template<typename TrackingServiceType, typename CreateServiceType, typename CreateServiceInterfaceType>
struct AsyncDependencyTrackerService final : public IAsyncDependencyTrackerService, public AdvancedService<AsyncDependencyTrackerService<TrackingServiceType, CreateServiceType, CreateServiceInterfaceType>> {
    AsyncDependencyTrackerService(DependencyRegister &reg, Properties props) : AdvancedService<AsyncDependencyTrackerService<TrackingServiceType, CreateServiceType, CreateServiceInterfaceType>>(std::move(props)) {
    }
    ~AsyncDependencyTrackerService() final = default;
    Task<tl::expected<void, Ichor::StartError>> start() final {
        _trackerRegistration = GetThreadLocalManager().template registerDependencyTracker<TrackingServiceType>(this, this);
        co_return {};
    }
    Task<void> stop() final {
        running = false;
        co_return;
    }

    AsyncGenerator<IchorBehaviour> handleDependencyRequest(v1::AlwaysNull<TrackingServiceType*>, DependencyRequestEvent const &evt) {
        co_await *_evt;

        auto svc = _services.find(evt.originatingService);

        if (svc != end(_services)) {
            co_return {};
        }

        Properties props{};
        auto newService = GetThreadLocalManager().template createServiceManager<CreateServiceType, CreateServiceInterfaceType>(std::move(props), evt.priority);
        _services.emplace(evt.originatingService, newService->getServiceId());

        co_return {};
    }

    AsyncGenerator<IchorBehaviour> handleDependencyUndoRequest(v1::AlwaysNull<TrackingServiceType*>, DependencyUndoRequestEvent const &evt) {
        co_await *_evt;

        auto service = _services.find(evt.originatingService);

        if(service != end(_services)) {
            co_await GetThreadLocalManager().template pushPrioritisedEventAsync<StopServiceEvent>(AdvancedService<AsyncDependencyTrackerService<TrackingServiceType, CreateServiceType, CreateServiceInterfaceType>>::getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, true, service->second, true);
            _services.erase(service);
        }

        co_return {};
    }

    [[nodiscard]] unordered_map<ServiceIdType, ServiceIdType, ServiceIdHash> getCreatedServices() noexcept final {
        return _services;
    }

    DependencyTrackerRegistration _trackerRegistration{};
    unordered_map<ServiceIdType, ServiceIdType, ServiceIdHash> _services;
    bool running{};
};
