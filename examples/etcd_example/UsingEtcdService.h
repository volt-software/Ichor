#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/services/etcd/IEtcd.h>
#include <ichor/dependency_management/IService.h>
#include <ichor/event_queues/IEventQueue.h>
#include <ichor/events/RunFunctionEvent.h>

using namespace Ichor;

class UsingEtcdV2Service final {
public:
    UsingEtcdV2Service(IService *self, IEventQueue *queue, ILogger *logger, IEtcd *EtcdV2Service) {
        ICHOR_LOG_INFO(logger, "UsingEtcdV2Service started");
        queue->pushEvent<RunFunctionEventAsync>(0, [logger, EtcdV2Service, queue, self]() -> AsyncGenerator<IchorBehaviour> {
            auto ret = co_await EtcdV2Service->put("test", "2");
            if(ret) {
                ICHOR_LOG_INFO(logger, "Successfully put key/value into etcd");
                auto storedVal = co_await EtcdV2Service->get("test");
                if(storedVal && storedVal->node && storedVal->node->value == "2") {
                    ICHOR_LOG_INFO(logger, "Successfully retrieved key/value into etcd");
                } else {
                    ICHOR_LOG_ERROR(logger, "Error retrieving key/value into etcd");
                }
            } else {
                ICHOR_LOG_ERROR(logger, "Error putting key/value into etcd: {}", ret.error());
            }

            queue->pushEvent<QuitEvent>(self->getServiceId());

            co_return IchorBehaviour::DONE;
        });
    }
};
