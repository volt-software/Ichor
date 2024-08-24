#pragma once

using namespace Ichor;

struct IDependencyTrackerService {
    [[nodiscard]] virtual unordered_map<uint64_t, ServiceIdType> getCreatedServices() noexcept = 0;
protected:
    ~IDependencyTrackerService() = default;
};

template<typename TrackingServiceType, typename CreateServiceType, typename CreateServiceInterfaceType>
struct DependencyTrackerService final : public IDependencyTrackerService, public AdvancedService<DependencyTrackerService<TrackingServiceType, CreateServiceType, CreateServiceInterfaceType>> {
    DependencyTrackerService(DependencyRegister &reg, Properties props) : AdvancedService<DependencyTrackerService<TrackingServiceType, CreateServiceType, CreateServiceInterfaceType>>(std::move(props)) {
    }
    ~DependencyTrackerService() final = default;
    Task<tl::expected<void, Ichor::StartError>> start() final {
        _trackerRegistration = GetThreadLocalManager().template registerDependencyTracker<TrackingServiceType>(this, this);
        co_return {};
    }
    Task<void> stop() final {
        running = false;
        co_return;
    }

    void handleDependencyRequest(AlwaysNull<TrackingServiceType*>, DependencyRequestEvent const &evt) {
        auto svc = _services.find(evt.originatingService);

        if (svc != end(_services)) {
            return;
        }

        Properties props{};
        auto newService = GetThreadLocalManager().template createServiceManager<CreateServiceType, CreateServiceInterfaceType>(std::move(props), evt.priority);
        _services.emplace(evt.originatingService, newService->getServiceId());
    }

    void handleDependencyUndoRequest(AlwaysNull<TrackingServiceType*>, DependencyUndoRequestEvent const &evt) {
        auto service = _services.find(evt.originatingService);

        if(service != end(_services)) {
            GetThreadLocalEventQueue().template pushPrioritisedEvent<StopServiceEvent>(AdvancedService<DependencyTrackerService<TrackingServiceType, CreateServiceType, CreateServiceInterfaceType>>::getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, service->second, true);
            _services.erase(service);
        }
    }

    [[nodiscard]] unordered_map<uint64_t, ServiceIdType> getCreatedServices() noexcept final {
        return _services;
    }

    DependencyTrackerRegistration _trackerRegistration{};
    unordered_map<uint64_t, ServiceIdType> _services;
    bool running{};
};
