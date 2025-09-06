#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>

using namespace Ichor;
using namespace Ichor::v1;

class DebugService final : public AdvancedService<DebugService> {
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

    void printServices() {
        auto svcs = GetThreadLocalManager().getAllServices();
        for(auto &[id, svc] : svcs) {
            ICHOR_LOG_INFO(_logger, "Svc {}:{} {}", svc->getServiceId(), svc->getServiceName(), svc->getServiceState());

            ICHOR_LOG_INFO(_logger, "\tInterfaces:");
            for(auto &iface : GetThreadLocalManager().getProvidedInterfacesForService(svc->getServiceId())) {
                ICHOR_LOG_INFO(_logger, "\t\tInterface {} hash {}", iface.getInterfaceName(), iface.interfaceNameHash);
            }

            ICHOR_LOG_INFO(_logger, "\tProperties:");
            for(auto &[key, val] : svc->getProperties()) {
                ICHOR_LOG_INFO(_logger, "\t\tProperty {} value {} size {} type {}", key, val, val.get_size(), val.type_name());
            }

            auto deps = GetThreadLocalManager().getDependencyRequestsForService(svc->getServiceId());
            ICHOR_LOG_INFO(_logger, "\tDependencies:");
            for(auto &dep : deps) {
                ICHOR_LOG_INFO(_logger, "\t\tDependency {} flags {} satisfied {}", dep.getInterfaceName(), dep.flags, dep.satisfied);
            }

            auto dependants = GetThreadLocalManager().getDependentsForService(svc->getServiceId());
            ICHOR_LOG_INFO(_logger, "\tUsed by:");
            for(auto &dep : dependants) {
                ICHOR_LOG_INFO(_logger, "\t\tDependant {}:{}", dep->getServiceId(), dep->getServiceName());
            }

            auto trackers = GetThreadLocalManager().getTrackersForService(svc->getServiceId());
            ICHOR_LOG_INFO(_logger, "\tTrackers:");
            for(auto &tracker : trackers) {
                ICHOR_LOG_INFO(_logger, "\t\tTracker for interface {} hash {}", tracker.interfaceName, tracker.interfaceNameHash);
            }

            ICHOR_LOG_INFO(_logger, "");
        }

        ICHOR_LOG_INFO(_logger, "\n===================================\n");
    }

    void addDependencyInstance(ILogger &logger, IService &) {
        _logger = &logger;
    }

    void removeDependencyInstance(ILogger &logger, IService&) {
        _logger = nullptr;
    }

    void addDependencyInstance(ITimerFactory &factory, IService &) {
        _timerFactory = &factory;
    }

    void removeDependencyInstance(ITimerFactory &factory, IService&) {
        _timerFactory = nullptr;
    }

    friend DependencyRegister;

    ILogger *_logger{};
    ITimerFactory *_timerFactory{};

};
