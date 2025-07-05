#pragma once

#include <utility>
#include <ichor/DependencyManager.h>
#include <ichor/Filter.h>
#include <ichor/services/logging/Logger.h>
#include "RuntimeCreatedService.h"

using namespace Ichor;
using namespace Ichor::v1;

// custom filter logic, to ensure a created RuntimeCreatedService only gets inserted into a `TestService` if the scope matches.
class ScopeFilterEntry final {
public:
    explicit ScopeFilterEntry(std::string _scope) : scope(std::move(_scope)) {}

    // The `manager` here is the Ichor-specific abstraction that encapsulates the requesting service. In this case, it would be the Manager for `TestService`.
    // The manager contains ways to get the assigned properties (which we use here to retrieve the scope), but also a list of dependencies the `TestService` requested.
    [[nodiscard]] bool matches(ILifecycleManager const &manager) const noexcept {
        auto const scopeProp = manager.getProperties().find("scope");

        return scopeProp != cend(manager.getProperties()) && Ichor::v1::any_cast<const std::string&>(scopeProp->second) == scope;
    }

    [[nodiscard]] std::string getDescription() const noexcept {
        std::string s;
        fmt::format_to(std::back_inserter(s), "ScopeFilterEntry {}", scope);
        return s;
    }

    std::string scope;
};

class FactoryService final {
public:
    FactoryService(DependencyManager *dm, IService *self, ILogger *logger) : _self(self), _logger(logger) {
        ICHOR_LOG_INFO(_logger, "FactoryService started");
        _trackerRegistration = dm->registerDependencyTracker<IRuntimeCreatedService>(this, self);
    }
    ~FactoryService() {
        ICHOR_LOG_INFO(_logger, "FactoryService stopped");
    }

private:
    // a service has been created and has requested an IRuntimeCreatedService
    AsyncGenerator<IchorBehaviour> handleDependencyRequest(v1::AlwaysNull<IRuntimeCreatedService*>, DependencyRequestEvent const &evt) {
        // What to do when the requesting service has no properties? In this case, we don't create anything, probably preventing the service from being started.
        if(!evt.properties.has_value()) {
            ICHOR_LOG_ERROR(_logger, "missing properties");
            co_return {};
        }

        auto scopeProp = evt.properties.value()->find("scope");

        if(scopeProp == end(*evt.properties.value())) {
            ICHOR_LOG_ERROR(_logger, "scope missing");
            co_return {};
        }

        auto const& scope = Ichor::v1::any_cast<const std::string&>(scopeProp->second);

        ICHOR_LOG_INFO(_logger, "Tracked IRuntimeCreatedService request for scope {}", scope);

        // See if we have already created a `RuntimeCreatedService` for this particular scope
        auto runtimeService = _scopedRuntimeServices.find(scope);

        if(runtimeService == end(_scopedRuntimeServices)) {
            auto newProps = *evt.properties.value();
            newProps.erase("Filter");
            // `Filter` is a magic keyword that Ichor uses to determine if this service is global or if Ichor should use its filtering logic.
            // In this case, we tell Ichor to only insert this service if the requesting service has a matching scope
            newProps.emplace("Filter", Ichor::v1::make_any<Filter>(ScopeFilterEntry{scope}));

            _scopedRuntimeServices.emplace(scope, GetThreadLocalManager().createServiceManager<RuntimeCreatedService, IRuntimeCreatedService>(std::move(newProps))->getServiceId());
        }

        co_return {};
    }

    // a service has been created but now has to be destroyed and requested an IRuntimeCreatedService
    AsyncGenerator<IchorBehaviour> handleDependencyUndoRequest(v1::AlwaysNull<IRuntimeCreatedService*>, DependencyUndoRequestEvent const &evt) {
        if(!evt.properties) {
            ICHOR_LOG_ERROR(_logger, "properties missing");
            co_return {};
        }

        auto &properties = (*evt.properties);
        auto scopeProp = properties.find("scope");

        if(scopeProp == end(properties)) {
            ICHOR_LOG_ERROR(_logger, "scope missing");
            co_return {};
        }

        auto const& scope = Ichor::v1::any_cast<const std::string&>(scopeProp->second);

        ICHOR_LOG_INFO(_logger, "Tracked IRuntimeCreatedService undo request for scope {}", scope);

        // This logic does not account for multiple services using the same scope
        // That is easily fixed by using a reference counter in the container, but is outside the scope of this example (pun intended)
        auto service = _scopedRuntimeServices.find(scope);
        if(service != end(_scopedRuntimeServices)) {
            GetThreadLocalEventQueue().pushPrioritisedEvent<StopServiceEvent>(_self->getServiceId(), INTERNAL_DEPENDENCY_EVENT_PRIORITY, service->second, true);
            _scopedRuntimeServices.erase(scope);
        }

        co_return {};
    }

    friend DependencyManager;

    IService *_self{};
    ILogger *_logger{};
    DependencyTrackerRegistration _trackerRegistration{};
    std::unordered_map<std::string, ServiceIdType> _scopedRuntimeServices{};
};
