#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/etcd/IEtcdV3.h>
#include <ichor/events/RunFunctionEvent.h>

namespace Ichor {
    struct Etcdv3UsingService final : public AdvancedService<Etcdv3UsingService> {
        Etcdv3UsingService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
            reg.registerDependency<Etcd::v3::IEtcd>(this, DependencyFlags::REQUIRED);
            reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
        }

        Task<tl::expected<void, Ichor::StartError>> start() final {
            _v = _etcd->getDetectedVersion();
            co_await put_get_delete_test();
            co_await txn_test();
            co_await leases_test();
            co_await compact_test();

            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());

            co_return {};
        }

        void addDependencyInstance(Etcd::v3::IEtcd &Etcd, IService&) {
            _etcd = &Etcd;
        }

        void removeDependencyInstance(Etcd::v3::IEtcd&, IService&) {
            _etcd = nullptr;
        }

        void addDependencyInstance(ILogger &logger, IService&) {
            _logger = &logger;
        }

        void removeDependencyInstance(ILogger &, IService&) {
            _logger = nullptr;
        }

        Task<void> put_get_delete_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            int64_t revision{};
            Etcd::v3::EtcdPutRequest putReq{"v3_test_key", "test_value", 0, tl::nullopt, tl::nullopt, tl::nullopt};

            auto putReply = co_await _etcd->put(putReq);
            if (!putReply) {
                throw std::runtime_error("put");
            }
            revision = putReply->header.revision;

            Etcd::v3::EtcdRangeRequest rangeReq{.key = "v3_test_key"};
            auto rangeReply = co_await _etcd->range(rangeReq);
            if (!rangeReply) {
                throw std::runtime_error("range");
            }

            if ((*rangeReply).kvs.size() != 1) {
                throw std::runtime_error("range size");
            }

            if ((*rangeReply).kvs[0].key != "v3_test_key") {
                throw std::runtime_error("range key");
            }

            if ((*rangeReply).kvs[0].value != "test_value") {
                throw std::runtime_error("range value");
            }

            Etcd::v3::EtcdDeleteRangeRequest deleteReq{.key = "v3_test_key"};

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

        Task<void> txn_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            int64_t revision{};
            Etcd::v3::EtcdPutRequest putReq{"v3_txn_key", "txn_value", tl::nullopt, tl::nullopt, tl::nullopt, tl::nullopt};

            auto putReply = co_await _etcd->put(putReq);
            if (!putReply) {
                throw std::runtime_error("put");
            }

            Etcd::v3::EtcdRangeRequest rangeReq{.key = "v3_txn_key"};
            auto rangeReply = co_await _etcd->range(rangeReq);
            if (!rangeReply) {
                throw std::runtime_error("range");
            }
            if (rangeReply->kvs.size() != 1) {
                throw std::runtime_error("range size");
            }

            revision = rangeReply->kvs[0].create_revision;

            Etcd::v3::EtcdTxnRequest txnReq{};
            txnReq.compare.emplace_back(Etcd::v3::EtcdCompare{.target = Etcd::v3::EtcdCompareTarget::CREATE,.key = "v3_txn_key", .create_revision = revision});
            txnReq.success.emplace_back(Etcd::v3::EtcdRequestOp{.request_range = Etcd::v3::EtcdRangeRequest{.key = "v3_txn_key"}});
            auto txnReply = co_await _etcd->txn(txnReq);
            if (!txnReply) {
                throw std::runtime_error("txn");
            }

            if(!txnReply->succeeded) {
                throw std::runtime_error("txn succeeded false");
            }

            if(txnReply->responses.size() != 1) {
                throw std::runtime_error("txn responses size != 1");
            }
        }

        Task<void> leases_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            Etcd::v3::LeaseGrantRequest grantReq{100, 101};

            auto grantReply = co_await _etcd->leaseGrant(grantReq);
            if (!grantReply) {
                throw std::runtime_error("grant");
            }

            Etcd::v3::LeaseKeepAliveRequest keepAliveReq{101};
            auto keepAliveReply = co_await _etcd->leaseKeepAlive(keepAliveReq);
            if (!keepAliveReply) {
                throw std::runtime_error("keepAlive");
            }
            if(keepAliveReply->result.id != 101) {
                throw std::runtime_error("keepAlive id != 101");
            }
            if(keepAliveReply->result.ttl_in_seconds <= 90) {
                throw std::runtime_error("keepAlive ttl_in_seconds <= 90");
            }

            Etcd::v3::EtcdPutRequest putReq{"v3_lease_key", "lease_value", 101, tl::nullopt, tl::nullopt, tl::nullopt};
            auto putReply = co_await _etcd->put(putReq);
            if (!putReply) {
                throw std::runtime_error("put");
            }

            Etcd::v3::LeaseTimeToLiveRequest ttlReq{101, true};
            auto ttlReply = co_await _etcd->leaseTimeToLive(ttlReq);
            if (!ttlReply) {
                throw std::runtime_error("ttl");
            }
            if(ttlReply->id != 101) {
                throw std::runtime_error("ttl id != 101");
            }
            if(ttlReply->ttl_in_seconds <= 90) {
                throw std::runtime_error("ttl ttl_in_seconds <= 90");
            }
            if(ttlReply->granted_ttl != 100) {
                throw std::runtime_error("ttl granted_ttl != 100");
            }
            if(ttlReply->keys.size() != 1) {
                throw std::runtime_error("ttl keys.size() != 1");
            }
            if(ttlReply->keys[0] != "v3_lease_key") {
                throw std::runtime_error("ttl keys[0] != v3_lease_key");
            }

            if(_v >= Version{3, 3, 0}) {
                Etcd::v3::LeaseLeasesRequest leasesReq{};
                auto leasesReply = co_await _etcd->leaseLeases(leasesReq);
                if (!leasesReply) {
                    throw std::runtime_error("leases");
                }
                if (leasesReply->leases.size() != 1) {
                    throw std::runtime_error("leases leases.size() != 1");
                }
                if (leasesReply->leases[0].id != 101) {
                    throw std::runtime_error("leases leases[0].id != 101");
                }
            }

            Etcd::v3::LeaseRevokeRequest revokeReq{101};
            auto revokeReply = co_await _etcd->leaseRevoke(revokeReq);
            if(!revokeReply) {
                throw std::runtime_error("revoke");
            }
        }

        Task<void> compact_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            int64_t revision{};
            Etcd::v3::EtcdPutRequest putReq{"v3_compaction_key", "compaction_value", tl::nullopt, tl::nullopt, tl::nullopt, tl::nullopt};

            auto putReply = co_await _etcd->put(putReq);
            if (!putReply) {
                throw std::runtime_error("put");
            }

            revision = putReply->header.revision;

            Etcd::v3::EtcdCompactionRequest compactionRequest{revision, tl::nullopt};

            auto compactionReply = co_await _etcd->compact(compactionRequest);
            if (!compactionReply) {
                throw std::runtime_error("compactionReply");
            }
        }

        Etcd::v3::IEtcd *_etcd;
        ILogger *_logger{};
        Version _v{};
    };
}
