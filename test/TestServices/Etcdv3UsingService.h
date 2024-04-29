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
            co_await put_get_delete_test();
            co_await txn_test();

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
            Etcd::v3::EtcdPutRequest putReq{"v3_txn_key", "txn_value", 0, tl::nullopt, tl::nullopt, tl::nullopt};

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
            //txnReq.failure.emplace_back(Etcd::v3::EtcdRequestOp{.request_range = Etcd::v3::EtcdRangeRequest{.key = "v3_txn_key"}});
            auto txnReply = co_await _etcd->txn(txnReq);
            if (!txnReply) {
                throw std::runtime_error("txn");
            }

            // something is broken with txn, probably has to do with the null fields that glaze adds
            if(txnReply->responses.size() != 1) {
                throw std::runtime_error("txn responses size != 1");
            }
        }
        Etcd::v3::IEtcd *_etcd;
        ILogger *_logger{};
        Version _v{};
    };
}
