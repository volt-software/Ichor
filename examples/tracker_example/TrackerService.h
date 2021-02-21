#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>

#include <utility>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include "RuntimeCreatedService.h"

using namespace Ichor;

class ScopeFilterEntry final {
public:
    explicit ScopeFilterEntry(std::string _scope) : scope(std::move(_scope)) {}
    explicit ScopeFilterEntry(const char *_scope) : scope(_scope) {}

    [[nodiscard]] bool matches(const std::shared_ptr<ILifecycleManager> &manager) const {
        auto const scopeProp = manager->getProperties()->find("scope");

        return scopeProp != end(*manager->getProperties()) && Ichor::any_cast<const std::string&>(scopeProp->second) == scope;
    }

    const std::string scope;
};

class TrackerService final : public Service<TrackerService> {
public:
    TrackerService(DependencyRegister &reg, IchorProperties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~TrackerService() final = default;
    bool start() final {
        ICHOR_LOG_INFO(_logger, "TrackerService started");
        _trackerRegistration = getManager()->registerDependencyTracker<IRuntimeCreatedService>(this);
        return true;
    }

    bool stop() final {
        ICHOR_LOG_INFO(_logger, "TrackerService stopped");
        _trackerRegistration = nullptr;
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;
        ICHOR_LOG_TRACE(_logger, "Inserted logger");
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

    void handleDependencyRequest(IRuntimeCreatedService*, DependencyRequestEvent const * const evt) {
        if(!evt->properties.has_value()) {
            ICHOR_LOG_ERROR(_logger, "missing properties");
            return;
        }

        auto scopeProp = evt->properties.value()->find("scope");

        if(scopeProp == end(*evt->properties.value())) {
            ICHOR_LOG_ERROR(_logger, "scope missing");
            return;
        }

        auto const& scope = Ichor::any_cast<const std::string&>(scopeProp->second);

        ICHOR_LOG_INFO(_logger, "Tracked IRuntimeCreatedService request for scope {}", scope);

        auto runtimeService = _scopedRuntimeServices.find(scope);

        if(runtimeService == end(_scopedRuntimeServices)) {
            auto newProps = *evt->properties.value();
            newProps.emplace("Filter", Ichor::make_any<Filter>(getMemoryResource(), Filter{getMemoryResource(), ScopeFilterEntry{scope}}));

            _scopedRuntimeServices.emplace(scope, getManager()->createServiceManager<RuntimeCreatedService, IRuntimeCreatedService>(std::move(newProps)));
        }
    }

    void handleDependencyUndoRequest(IRuntimeCreatedService*, DependencyUndoRequestEvent const * const evt) {
        auto scopeProp = evt->properties->find("scope");

        if(scopeProp == end(*evt->properties)) {
            ICHOR_LOG_ERROR(_logger, "scope missing");
            return;
        }

        auto const& scope = Ichor::any_cast<const std::string&>(scopeProp->second);

        ICHOR_LOG_INFO(_logger, "Tracked IRuntimeCreatedService undo request for scope {}", scope);

        auto service = _scopedRuntimeServices.find(scope);
        if(service != end(_scopedRuntimeServices)) {
            getManager()->pushEvent<RemoveServiceEvent>(evt->originatingService, service->second->getServiceId());
            _scopedRuntimeServices.erase(scope);
        }
    }

private:
    ILogger *_logger{nullptr};
    std::unique_ptr<DependencyTrackerRegistration, Deleter> _trackerRegistration{nullptr};
    std::unordered_map<std::string, RuntimeCreatedService*> _scopedRuntimeServices{};
};