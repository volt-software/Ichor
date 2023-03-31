#pragma once

#include <utility>
#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include "RuntimeCreatedService.h"

using namespace Ichor;

class ScopeFilterEntry final {
public:
    explicit ScopeFilterEntry(std::string _scope) : scope(std::move(_scope)) {}

    [[nodiscard]] bool matches(ILifecycleManager const &manager) const noexcept {
        auto const scopeProp = manager.getProperties().find("scope");

        return scopeProp != cend(manager.getProperties()) && Ichor::any_cast<const std::string&>(scopeProp->second) == scope;
    }

    std::string scope;
};

class TrackerService final {
public:
    TrackerService(DependencyManager *dm, IService *self, ILogger *logger) : _self(self), _logger(logger) {
        ICHOR_LOG_INFO(_logger, "TrackerService started");
        _trackerRegistration = dm->registerDependencyTracker<IRuntimeCreatedService>(this, self);
    }
    ~TrackerService() {
        ICHOR_LOG_INFO(_logger, "TrackerService stopped");
    }

private:
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
            GetThreadLocalEventQueue().pushEvent<StopServiceEvent>(_self->getServiceId(), service->second->getServiceId());
            GetThreadLocalEventQueue().pushPrioritisedEvent<RemoveServiceEvent>(_self->getServiceId(), INTERNAL_EVENT_PRIORITY + 11, service->second->getServiceId());
            _scopedRuntimeServices.erase(scope);
        }
    }

    friend DependencyManager;

    IService *_self{};
    ILogger *_logger{};
    DependencyTrackerRegistration _trackerRegistration{};
    std::unordered_map<std::string, IService*> _scopedRuntimeServices{};
};
