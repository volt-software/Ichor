#pragma once

#include <utility>
#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include "RuntimeCreatedService.h"

using namespace Ichor;

class ScopeFilterEntry final {
public:
    explicit ScopeFilterEntry(std::string _scope) : scope(std::move(_scope)) {}
    explicit ScopeFilterEntry(const char *_scope) : scope(_scope) {}

    [[nodiscard]] bool matches(ILifecycleManager const &manager) const noexcept {
        auto const scopeProp = manager.getProperties().find("scope");

        return scopeProp != cend(manager.getProperties()) && Ichor::any_cast<const std::string&>(scopeProp->second) == scope;
    }

    const std::string scope;
};

class TrackerService final : public AdvancedService<TrackerService> {
public:
    TrackerService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~TrackerService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "TrackerService started");
        _trackerRegistration = GetThreadLocalManager().registerDependencyTracker<IRuntimeCreatedService>(this);
        co_return {};
    }

    Task<void> stop() final {
        ICHOR_LOG_INFO(_logger, "TrackerService stopped");
        _trackerRegistration.reset();
        co_return;
    }

    void addDependencyInstance(ILogger &logger, IService &) {
        _logger = &logger;
    }

    void removeDependencyInstance(ILogger&, IService&) {
        _logger = nullptr;
    }

    void handleDependencyRequest(AlwaysNull<IRuntimeCreatedService*>, DependencyRequestEvent const &evt) {
        if(!evt.properties.has_value()) {
            ICHOR_LOG_ERROR(_logger, "missing properties");
            return;
        }

        auto scopeProp = evt.properties.value()->find("scope");

        if(scopeProp == end(*evt.properties.value())) {
            ICHOR_LOG_ERROR(_logger, "scope missing");
            return;
        }

        auto const& scope = Ichor::any_cast<const std::string&>(scopeProp->second);

        ICHOR_LOG_INFO(_logger, "Tracked IRuntimeCreatedService request for scope {}", scope);

        auto runtimeService = _scopedRuntimeServices.find(scope);

        if(runtimeService == end(_scopedRuntimeServices)) {
            auto newProps = *evt.properties.value();
            newProps.emplace("Filter", Ichor::make_any<Filter>(Filter{ScopeFilterEntry{scope}}));

            _scopedRuntimeServices.emplace(scope, GetThreadLocalManager().createServiceManager<RuntimeCreatedService, IRuntimeCreatedService>(std::move(newProps)));
        }
    }

    void handleDependencyUndoRequest(AlwaysNull<IRuntimeCreatedService*>, DependencyUndoRequestEvent const &evt) {
        auto scopeProp = evt.properties.find("scope");

        if(scopeProp == end(evt.properties)) {
            ICHOR_LOG_ERROR(_logger, "scope missing");
            return;
        }

        auto const& scope = Ichor::any_cast<const std::string&>(scopeProp->second);

        ICHOR_LOG_INFO(_logger, "Tracked IRuntimeCreatedService undo request for scope {}", scope);

        auto service = _scopedRuntimeServices.find(scope);
        if(service != end(_scopedRuntimeServices)) {
            // TODO: This only works for synchronous start/stop loggers, maybe turn into async and await a stop before removing?
            GetThreadLocalEventQueue().pushEvent<StopServiceEvent>(getServiceId(), service->second->getServiceId());
            GetThreadLocalEventQueue().pushPrioritisedEvent<RemoveServiceEvent>(getServiceId(), INTERNAL_EVENT_PRIORITY + 11, service->second->getServiceId());
            _scopedRuntimeServices.erase(scope);
        }
    }

    friend DependencyRegister;
    friend DependencyManager;

    ILogger *_logger{nullptr};
    DependencyTrackerRegistration _trackerRegistration{};
    std::unordered_map<std::string, RuntimeCreatedService*> _scopedRuntimeServices{};
};
