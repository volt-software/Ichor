#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/services/etcd/IEtcdService.h>
#include <ichor/event_queues/IEventQueue.h>

using namespace Ichor;

class UsingEtcdService final {
public:
    UsingEtcdService(IService *self, IEventQueue *queue, ILogger *logger, IEtcdService *etcdService) : _logger(logger) {
        ICHOR_LOG_INFO(_logger, "UsingEtcdService started");
        if(_etcd->put("test", "2")) {
            ICHOR_LOG_TRACE(_logger, "Succesfully put key/value into etcd");
            auto storedVal = _etcd->get("test");
            if(storedVal == "2") {
                ICHOR_LOG_TRACE(_logger, "Succesfully retrieved key/value into etcd");
            } else {
                ICHOR_LOG_ERROR(_logger, "Error retrieving key/value into etcd");
            }
        } else {
            ICHOR_LOG_ERROR(_logger, "Error putting key/value into etcd");
        }

        queue->pushEvent<QuitEvent>(self->getServiceId());
    }
    ~UsingEtcdService() {
        ICHOR_LOG_INFO(_logger, "UsingEtcdService stopped");
    }

private:
    ILogger *_logger{};
    IEtcdService *_etcd{};
};
