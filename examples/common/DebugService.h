#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/ServiceExecutionScope.h>

using namespace Ichor;
using namespace Ichor::v1;

struct IDebugService {
    virtual void printServices() = 0;
};

class DebugService final : public IDebugService, public AdvancedService<DebugService> {
public:
    DebugService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED, Properties{{"LogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_INFO)}});
        reg.registerDependency<ITimerFactory>(this, DependencyFlags::REQUIRED);
    }
    ~DebugService() final = default;
private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        auto _timer = _timerFactory->createTimer();
        _timer.setCallback([this]() {
            printServices();
            std::terminate();
        });
        _timer.setChronoInterval(1000ms);
        _timer.startTimer();

        // printServices();

        co_return {};
    }

    Task<void> stop() final {
        // the timer factory also stops all timers registered to this service
        co_return;
    }

    void printServices() final {
        auto svcs = GetThreadLocalManager().getAllServices();
        for(auto &[id, svc] : svcs) {
            ICHOR_EMERGENCY_LOG2(_logger, "Svc {}:{} {}", svc->getServiceId(), svc->getServiceName(), svc->getServiceState());

            ICHOR_EMERGENCY_LOG1(_logger, "\tInterfaces:");
            for(auto &iface : GetThreadLocalManager().getProvidedInterfacesForService(svc->getServiceId())) {
                ICHOR_EMERGENCY_LOG2(_logger, "\t\tInterface {} hash {}", iface.getInterfaceName(), iface.interfaceNameHash);
            }

            ICHOR_EMERGENCY_LOG1(_logger, "\tProperties:");
            for(auto &[key, val] : svc->getProperties()) {
                ICHOR_EMERGENCY_LOG2(_logger, "\t\tProperty {} value {} size {} type {}", key, val, val.get_size(), val.type_name());
            }

            auto deps = GetThreadLocalManager().getDependencyRequestsForService(svc->getServiceId());
            ICHOR_EMERGENCY_LOG1(_logger, "\tDependencies:");
            for(auto &dep : deps) {
                ICHOR_EMERGENCY_LOG2(_logger, "\t\tDependency {} flags {} satisfied {}", dep.getInterfaceName(), dep.flags, dep.satisfied);
            }

            auto dependants = GetThreadLocalManager().getDependentsForService(svc->getServiceId());
            ICHOR_EMERGENCY_LOG1(_logger, "\tUsed by:");
            for(auto dep : dependants) {
                ICHOR_EMERGENCY_LOG2(_logger, "\t\tDependant {}:{}", dep->getServiceId(), dep->getServiceName());
            }

            auto trackers = GetThreadLocalManager().getTrackersForService(svc->getServiceId());
            ICHOR_EMERGENCY_LOG1(_logger, "\tTrackers:");
            for(auto tracker : trackers) {
                ICHOR_EMERGENCY_LOG2(_logger, "\t\tTracker for interface {} hash {}", tracker.interfaceName, tracker.interfaceNameHash);
            }

            ICHOR_EMERGENCY_LOG1(_logger, "");
        }

        ICHOR_EMERGENCY_LOG1(_logger, "\n===================================\n");

        auto serviceIdsWithActiveCoroutines = GetThreadLocalManager().getServiceIdsWhichHaveActiveCoroutines();
        ICHOR_EMERGENCY_LOG1(_logger, "Services With Active Coroutines:");
        for(auto const &[evt, svcIds] : serviceIdsWithActiveCoroutines) {
            ICHOR_EMERGENCY_LOG2(_logger, "\tevent {}:{}", evt.get_name(), evt.originatingService);
            for(auto const svcId : svcIds) {
                auto svc = GetThreadLocalManager().getIService(svcId);

                if(!svc) {
                    ICHOR_EMERGENCY_LOG2(_logger, "\t\tSvc {} but missing from manager", svcId);
                } else {
                    ICHOR_EMERGENCY_LOG2(_logger, "\t\tSvc {}:{} {}", (*svc)->getServiceId(), (*svc)->getServiceName(), (*svc)->getServiceState());
                }
            }
        }


        ICHOR_EMERGENCY_LOG1(_logger, "\n===================================\n");
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService &) {
        _logger = std::move(logger);
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService&) {
        _logger.reset();
    }

    void addDependencyInstance(Ichor::ScopedServiceProxy<ITimerFactory*> factory, IService &) {
        _timerFactory = std::move(factory);
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<ITimerFactory*> factory, IService&) {
        _timerFactory.reset();
    }

    friend DependencyRegister;

    Ichor::ScopedServiceProxy<ILogger*> _logger {};
    Ichor::ScopedServiceProxy<ITimerFactory*> _timerFactory {};

};
