#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>
#include <ichor/event_queues/IEventQueue.h>
#include <ichor/DependencyManager.h>
#include <thread>

using namespace Ichor;
using namespace Ichor::v1;

class IntrospectionService final {
public:
    IntrospectionService(IService *self, IEventQueue *queue, DependencyManager *dm, ILogger *logger, ITimerFactory *) : _logger(logger) {
        ICHOR_LOG_INFO(_logger, "IntrospectionService started");

        auto svcs = dm->getAllServices();

        for(auto &[svcId, svc] : svcs) {
            ICHOR_LOG_INFO(_logger, "Service {}:{}, guid {} priority {} state {}", svc->getServiceId(), svc->getServiceName(), svc->getServiceGid(), svc->getServicePriority(), svc->getServiceState());

            ICHOR_LOG_INFO(_logger, "\tInterfaces:");
            for(auto &iface : dm->getProvidedInterfacesForService(svc->getServiceId())) {
                ICHOR_LOG_INFO(_logger, "\t\tInterface {} hash {}", iface.getInterfaceName(), iface.interfaceNameHash);
            }

            ICHOR_LOG_INFO(_logger, "\tProperties:");
            for(auto &[key, val] : svc->getProperties()) {
                ICHOR_LOG_INFO(_logger, "\t\tProperty {} value {} size {} type {}", key, val, val.get_size(), val.type_name());
            }

            auto deps = dm->getDependencyRequestsForService(svc->getServiceId());
            ICHOR_LOG_INFO(_logger, "\tDependencies:");
            for(auto &dep : deps) {
                ICHOR_LOG_INFO(_logger, "\t\tDependency {} flags {} satisfied {}", dep.getInterfaceName(), dep.flags, dep.satisfied);
            }

            auto dependants = dm->getDependentsForService(svc->getServiceId());
            ICHOR_LOG_INFO(_logger, "\tUsed by:");
            for(auto &dep : dependants) {
                ICHOR_LOG_INFO(_logger, "\t\tDependant {}:{}", dep->getServiceId(), dep->getServiceName());
            }

            auto trackers = dm->getTrackersForService(svc->getServiceId());
            ICHOR_LOG_INFO(_logger, "\tTrackers:");
            for(auto &tracker : trackers) {
                ICHOR_LOG_INFO(_logger, "\t\tTracker for interface {} hash {}", tracker.interfaceName, tracker.interfaceNameHash);
            }

            ICHOR_LOG_INFO(_logger, "");

        }

        // TODO set logger of other service

        queue->pushEvent<QuitEvent>(self->getServiceId());
    }
    ~IntrospectionService() {
        ICHOR_LOG_INFO(_logger, "IntrospectionService stopped");
    }

private:
    ILogger *_logger;
};
