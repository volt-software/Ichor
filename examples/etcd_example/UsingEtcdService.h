#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/services/etcd/IEtcdV2.h>
#include <ichor/dependency_management/IService.h>
#include <ichor/event_queues/IEventQueue.h>
#include <ichor/events/RunFunctionEvent.h>

using namespace Ichor;
using namespace Ichor::v1;
using namespace Ichor::Etcdv2::v1;

class UsingEtcdV2Service final {
public:
    UsingEtcdV2Service(IService *self, IEventQueue *queue, ScopedServiceProxy<ILogger> logger, ScopedServiceProxy<IEtcd> EtcdV2Service) {
        ICHOR_LOG_INFO(logger, "UsingEtcdV2Service started");

        // standard put/get
        queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, [logger, EtcdV2Service]() -> AsyncGenerator<IchorBehaviour> {
            auto ret = co_await EtcdV2Service->put("test", "2", 10u); // put with a TTL of 10 seconds
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

            co_return {};
        });

        // wait for update and set
        queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, [logger, EtcdV2Service, queue, self]() -> AsyncGenerator<IchorBehaviour> {
            auto ret = co_await EtcdV2Service->put("watch", "3", 10u); // set value
            if(!ret) {
                std::terminate();
            }

            // update the "test" key in 250 ms
            auto start = std::chrono::steady_clock::now();
            queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, [EtcdV2Service, start]() -> AsyncGenerator<IchorBehaviour> {
                auto now = std::chrono::steady_clock::now();
                while(now - start < std::chrono::milliseconds(250)) {
                    co_yield IchorBehaviour::DONE;
                    now = std::chrono::steady_clock::now();
                }

                auto ret2 = co_await EtcdV2Service->put("watch", "4", 10u); // update value, this should trigger the watch
                if(!ret2) {
                    std::terminate();
                }

                co_return {};
            });

            // wait for a reply, this blocks this async event (hence why we need to update the key in another async event).
            // Note that this does not block the thread, just this event.
            auto getReply = co_await EtcdV2Service->get("watch", false, false, true, {});
            if(!getReply) {
                std::terminate();
            }

            if(getReply.value().node->value != "4") {
                std::terminate();
            }
            ICHOR_LOG_ERROR(logger, "Successfully got a value when watching 'watch'", ret.error());

            queue->pushEvent<QuitEvent>(self->getServiceId());

            co_return {};
        });
    }
};
