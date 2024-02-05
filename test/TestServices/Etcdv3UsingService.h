#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/etcd/IEtcdV3.h>
#include <ichor/events/RunFunctionEvent.h>

namespace Ichor {
    struct Etcdv3UsingService final : public AdvancedService<Etcdv3UsingService> {
        Etcdv3UsingService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
            reg.registerDependency<Etcd::v3::IEtcd>(this, DependencyFlags::REQUIRED);
        }

        Task<tl::expected<void, Ichor::StartError>> start() final {
            int64_t revision{};
            // standard create and get test
            {
                Etcd::v3::EtcdPutRequest putReq{"test_key", "test_value", 0, std::nullopt, std::nullopt, std::nullopt};

                auto putReply = co_await _etcd->put(putReq);
                if (!putReply) {
                    throw std::runtime_error("put");
                }
                revision = putReply->header.revision;

                Etcd::v3::EtcdRangeRequest rangeReq{};
                rangeReq.key = "test_key";
                auto rangeReply = co_await _etcd->range(rangeReq);
                if (!rangeReply) {
                    throw std::runtime_error("range");
                }

                if ((*rangeReply).kvs.size() != 1) {
                    throw std::runtime_error("range size");
                }

                if ((*rangeReply).kvs[0].key != "test_key") {
                    throw std::runtime_error("range key");
                }

                if ((*rangeReply).kvs[0].value != "test_value") {
                    throw std::runtime_error("range value");
                }

                Etcd::v3::EtcdDeleteRangeRequest deleteReq{};
                deleteReq.key = "test_key";

                auto deleteReply = co_await _etcd->deleteRange(deleteReq);
                if (!deleteReply) {
                    throw std::runtime_error("delete");
                }

                if(deleteReply->deleted != 1) {
                    throw std::runtime_error("delete deleted != 1");
                }

                if(deleteReply->header.revision != revision + 1) {
                    throw std::runtime_error("revision");
                }
            }

            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());

            co_return {};
        }

        void addDependencyInstance(Etcd::v3::IEtcd &Etcd, IService&) {
            _etcd = &Etcd;
        }

        void removeDependencyInstance(Etcd::v3::IEtcd&, IService&) {
            _etcd = nullptr;
        }

        Etcd::v3::IEtcd *_etcd;
    };
}
