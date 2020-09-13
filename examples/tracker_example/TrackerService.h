#pragma once

#include <cppelix/DependencyManager.h>
#include <cppelix/optional_bundles/logging_bundle/Logger.h>

#include <utility>
#include <cppelix/Service.h>
#include <cppelix/LifecycleManager.h>
#include "RuntimeCreatedService.h"

using namespace Cppelix;

class ScopeFilterEntry final {
public:
    explicit ScopeFilterEntry(std::string _scope) : scope(std::move(_scope)) {}
    explicit ScopeFilterEntry(const char *_scope) : scope(_scope) {}

    [[nodiscard]] bool matches(const std::shared_ptr<ILifecycleManager> &manager) const {
        auto scopeProp = manager->getProperties()->find("scope");

        return scopeProp != end(*manager->getProperties()) && std::any_cast<std::string>(scopeProp->second) == scope;
    }

    const std::string scope;
};

struct ITrackerService : virtual public IService {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class TrackerService final : public ITrackerService, public Service {
public:
    TrackerService(DependencyRegister &reg, CppelixProperties props) : Service(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
    }
    ~TrackerService() final = default;
    bool start() final {
        LOG_INFO(_logger, "TrackerService started");
        _trackerRegistration = getManager()->registerDependencyTracker<IRuntimeCreatedService>(getServiceId(), this);
        return true;
    }

    bool stop() final {
        LOG_INFO(_logger, "TrackerService stopped");
        _trackerRegistration = nullptr;
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;
        LOG_TRACE(_logger, "Inserted logger");
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

    void handleDependencyRequest(IRuntimeCreatedService*, DependencyRequestEvent const * const evt) {
        if(!evt->properties.has_value()) {
            LOG_ERROR(_logger, "missing properties");
            return;
        }

        auto scopeProp = evt->properties.value()->find("scope");

        if(scopeProp == end(*evt->properties.value())) {
            LOG_ERROR(_logger, "scope missing");
            return;
        }

        auto scope = std::any_cast<std::string>(scopeProp->second);

        LOG_INFO(_logger, "Tracked IRuntimeCreatedService request for scope {}", scope);

        auto runtimeService = _scopedRuntimeServices.find(scope);

        if(runtimeService == end(_scopedRuntimeServices)) {
            auto newProps = *evt->properties.value();
            newProps.emplace("Filter", Filter{ScopeFilterEntry{scope}});

            _scopedRuntimeServices.emplace(scope, getManager()->createServiceManager<RuntimeCreatedService, IRuntimeCreatedService>(newProps));
        }
    }

    void handleDependencyUndoRequest(IRuntimeCreatedService*, DependencyUndoRequestEvent const * const evt) {
        auto scopeProp = evt->properties->find("scope");

        if(scopeProp == end(*evt->properties)) {
            LOG_ERROR(_logger, "scope missing");
            return;
        }

        auto scope = std::any_cast<std::string>(scopeProp->second);

        LOG_INFO(_logger, "Tracked IRuntimeCreatedService undo request for scope {}", scope);

        auto service = _scopedRuntimeServices.find(scope);
        if(service != end(_scopedRuntimeServices)) {
            getManager()->pushEvent<RemoveServiceEvent>(evt->originatingService, service->second->getServiceId());
            _scopedRuntimeServices.erase(scope);
        }
    }

private:
    ILogger *_logger;
    std::unique_ptr<DependencyTrackerRegistration> _trackerRegistration;
    std::unordered_map<std::string, RuntimeCreatedService*> _scopedRuntimeServices;
};