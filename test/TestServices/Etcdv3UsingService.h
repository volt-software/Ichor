#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/etcd/IEtcdV3.h>
#include <ichor/events/RunFunctionEvent.h>

using namespace Ichor::v1;

namespace Ichor {
    struct Etcdv3UsingService final : public AdvancedService<Etcdv3UsingService> {
        Etcdv3UsingService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
            reg.registerDependency<Etcdv3::v1::IEtcd>(this, DependencyFlags::REQUIRED);
            reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
        }

        Task<tl::expected<void, Ichor::StartError>> start() final {
            _v = _etcd->getDetectedVersion();
            co_await put_get_delete_test();
            co_await txn_test();
            co_await leases_test();
            co_await users_test();
            co_await roles_test();
            co_await compact_test();

            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());

            co_return {};
        }

        void addDependencyInstance(Etcdv3::v1::IEtcd &Etcd, IService&) {
            _etcd = &Etcd;
        }

        void removeDependencyInstance(Etcdv3::v1::IEtcd&, IService&) {
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
            Etcdv3::v1::EtcdPutRequest putReq{"v3_test_key", "test_value", 0, tl::nullopt, tl::nullopt, tl::nullopt};

            auto putReply = co_await _etcd->put(putReq);
            if (!putReply) {
                throw std::runtime_error("put");
            }
            revision = putReply->header.revision;

            Etcdv3::v1::EtcdRangeRequest rangeReq{.key = "v3_test_key"};
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

            Etcdv3::v1::EtcdDeleteRangeRequest deleteReq{.key = "v3_test_key"};

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
            Etcdv3::v1::EtcdPutRequest putReq{"v3_txn_key", "txn_value", tl::nullopt, tl::nullopt, tl::nullopt, tl::nullopt};

            auto putReply = co_await _etcd->put(putReq);
            if (!putReply) {
                throw std::runtime_error("put");
            }

            Etcdv3::v1::EtcdRangeRequest rangeReq{.key = "v3_txn_key"};
            auto rangeReply = co_await _etcd->range(rangeReq);
            if (!rangeReply) {
                throw std::runtime_error("range");
            }
            if (rangeReply->kvs.size() != 1) {
                throw std::runtime_error("range size");
            }

            revision = rangeReply->kvs[0].create_revision;

            Etcdv3::v1::EtcdTxnRequest txnReq{};
            txnReq.compare.emplace_back(Etcdv3::v1::EtcdCompare{.target = Etcdv3::v1::EtcdCompareTarget::CREATE,.key = "v3_txn_key", .create_revision = revision});
            txnReq.success.emplace_back(Etcdv3::v1::EtcdRequestOp{.request_range = Etcdv3::v1::EtcdRangeRequest{.key = "v3_txn_key"}});
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
            Etcdv3::v1::LeaseGrantRequest grantReq{100, 101};

            auto grantReply = co_await _etcd->leaseGrant(grantReq);
            if (!grantReply) {
                throw std::runtime_error("grant");
            }

            Etcdv3::v1::LeaseKeepAliveRequest keepAliveReq{101};
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

            Etcdv3::v1::EtcdPutRequest putReq{"v3_lease_key", "lease_value", 101, tl::nullopt, tl::nullopt, tl::nullopt};
            auto putReply = co_await _etcd->put(putReq);
            if (!putReply) {
                throw std::runtime_error("put");
            }

            Etcdv3::v1::LeaseTimeToLiveRequest ttlReq{101, true};
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
                Etcdv3::v1::LeaseLeasesRequest leasesReq{};
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

            Etcdv3::v1::LeaseRevokeRequest revokeReq{101};
            auto revokeReply = co_await _etcd->leaseRevoke(revokeReq);
            if(!revokeReply) {
                throw std::runtime_error("revoke");
            }
        }

        Task<void> users_test() const {
            ICHOR_LOG_INFO(_logger, "running test");

            Etcdv3::v1::AuthUserGetRequest userGetReq{"v3_test_user"};
            auto userGetReply = co_await _etcd->userGet(userGetReq);
            if(userGetReply) {
                Etcdv3::v1::AuthUserDeleteRequest userDelReq{"v3_test_user"};
                auto userDelReply = co_await _etcd->userDelete(userDelReq);
                if(!userDelReply) {
                    throw std::runtime_error("userDel at init");
                }
            }

            Etcdv3::v1::AuthUserAddRequest userAddReq{"v3_test_user", "v3_test_password", {}, {}};
            auto userAddReply = co_await _etcd->userAdd(userAddReq);
            if (!userAddReply) {
                throw std::runtime_error("userAdd");
            }

            Etcdv3::v1::AuthUserListRequest userListReq{};
            auto userListReply = co_await _etcd->userList(userListReq);
            if (!userListReply) {
                throw std::runtime_error("userList");
            }
            if(std::find(userListReply->users.begin(), userListReply->users.end(), "v3_test_user") == userListReply->users.end()) {
                throw std::runtime_error("v3_test_user not in list user");
            }
            if(std::find(userListReply->users.begin(), userListReply->users.end(), "root") == userListReply->users.end()) {
                userAddReq.name = "root";
                userAddReq.password = "root";
                userAddReply = co_await _etcd->userAdd(userAddReq);
                if (!userAddReply) {
                    throw std::runtime_error("userAdd root");
                }

                Etcdv3::v1::AuthUserGrantRoleRequest userGrantRoleReq{"root", "root"};
                auto userGrantRoleReply = co_await _etcd->userGrantRole(userGrantRoleReq);
                if(!userGrantRoleReply) {
                    throw std::runtime_error("grant role root");
                }
            }

            Etcdv3::v1::AuthUserGrantRoleRequest userGrantRoleReq{"v3_test_user", "root"};
            auto userGrantRoleReply = co_await _etcd->userGrantRole(userGrantRoleReq);
            if(!userGrantRoleReply) {
                throw std::runtime_error("grant role");
            }

            userGetReply = co_await _etcd->userGet(userGetReq);
            if(!userGetReply) {
                throw std::runtime_error("userGetReply");
            }

            if(std::find(userGetReply->roles.begin(), userGetReply->roles.end(), "root") == userGetReply->roles.end()) {
                throw std::runtime_error("root not in user's role list");
            }

            Etcdv3::v1::AuthEnableRequest authEnableReq{};
            auto authEnableReply = co_await _etcd->authEnable(authEnableReq);
            if(_v < Version{3, 3, 0} && authEnableReply) {
                throw std::runtime_error("auth enable");
            }
            if(_v >= Version{3, 3, 0} && !authEnableReply) {
                throw std::runtime_error("auth enable");
            }

            Etcdv3::v1::AuthenticateRequest userAuthReq{"v3_test_user", "v3_test_password"};
            auto userAuthReply = co_await _etcd->authenticate(userAuthReq);
            if(_v < Version{3, 3, 0} && userAuthReply) {
                throw std::runtime_error("authenticate");
            }
            if(_v >= Version{3, 3, 0} && !userAuthReply) {
                throw std::runtime_error("authenticate");
            }
            if(_v >= Version{3, 3, 0} && _etcd->getAuthenticationUser() != "v3_test_user") {
                throw std::runtime_error("authenticate user");
            }

            Etcdv3::v1::AuthDisableRequest authDisableReq{};
            auto authDisableReply = co_await _etcd->authDisable(authDisableReq);
            if(_v < Version{3, 3, 0} && authDisableReply) {
                throw std::runtime_error("auth disable");
            }
            if(_v >= Version{3, 3, 0} && !authDisableReply) {
                throw std::runtime_error("auth disable");
            }

            Etcdv3::v1::AuthUserChangePasswordRequest authChangePassReq{"v3_test_user", "v3_test_password2"};
            auto authChangePassReply = co_await _etcd->userChangePassword(authChangePassReq);
            if(!authChangePassReply) {
                throw std::runtime_error("auth change pass");
            }

            authEnableReply = co_await _etcd->authEnable(authEnableReq);
            if(_v < Version{3, 3, 0} && authEnableReply) {
                throw std::runtime_error("auth enable2");
            }
            if(_v >= Version{3, 3, 0} && !authEnableReply) {
                throw std::runtime_error("auth enable2");
            }

            userAuthReq.password = "v3_test_password2";
            userAuthReply = co_await _etcd->authenticate(userAuthReq);
            if(_v < Version{3, 3, 0} && userAuthReply) {
                throw std::runtime_error("authenticate2");
            }
            if(_v >= Version{3, 3, 0} && !userAuthReply) {
                throw std::runtime_error("authenticate2");
            }
            if(_v >= Version{3, 3, 0} && _etcd->getAuthenticationUser() != "v3_test_user") {
                throw std::runtime_error("authenticate user2");
            }

            authDisableReply = co_await _etcd->authDisable(authDisableReq);
            if(_v < Version{3, 3, 0} && authDisableReply) {
                throw std::runtime_error("auth disable2");
            }
            if(_v >= Version{3, 3, 0} && !authDisableReply) {
                throw std::runtime_error("auth disable2");
            }



            Etcdv3::v1::AuthUserRevokeRoleRequest userRevokeRoleReq{"v3_test_user", "root"};
            auto userRevokeRoleReply = co_await _etcd->userRevokeRole(userRevokeRoleReq);
            if(!userRevokeRoleReply) {
                throw std::runtime_error("revoke role");
            }

            userGetReply = co_await _etcd->userGet(userGetReq);
            if(!userGetReply) {
                throw std::runtime_error("userGetReply");
            }

            if(std::find(userGetReply->roles.begin(), userGetReply->roles.end(), "root") != userGetReply->roles.end()) {
                throw std::runtime_error("root still in user's role list");
            }

            Etcdv3::v1::AuthUserDeleteRequest userDelReq{"v3_test_user"};
            auto userDelReply = co_await _etcd->userDelete(userDelReq);
            if(!userDelReply) {
                throw std::runtime_error("userDel");
            }
        }

        Task<void> roles_test() const {
            ICHOR_LOG_INFO(_logger, "running test");

            Etcdv3::v1::AuthRoleGetRequest roleGetReq{"v3_test_role"};
            auto roleGetReply = co_await _etcd->roleGet(roleGetReq);
            if(roleGetReply) {
                Etcdv3::v1::AuthRoleDeleteRequest roleDelReq{"v3_test_role"};
                auto roleDelReply = co_await _etcd->roleDelete(roleDelReq);
                if(!roleDelReply) {
                    throw std::runtime_error("roleDel at init");
                }
            }

            Etcdv3::v1::AuthRoleAddRequest roleAddReq{"v3_test_role"};
            auto roleAddReply = co_await _etcd->roleAdd(roleAddReq);
            if (!roleAddReply) {
                throw std::runtime_error("roleAdd");
            }

            Etcdv3::v1::AuthRoleListRequest roleListReq{};
            auto roleListReply = co_await _etcd->roleList(roleListReq);
            if (!roleListReply) {
                throw std::runtime_error("roleList");
            }
            if(std::find(roleListReply->roles.begin(), roleListReply->roles.end(), "v3_test_role") == roleListReply->roles.end()) {
                throw std::runtime_error("v3_test_role not in list role");
            }

            Etcdv3::v1::AuthRoleGrantPermissionRequest roleGrantPermissionReq{"v3_test_role", Etcdv3::v1::AuthPermission{Etcdv3::v1::EtcdAuthPermissionType::WRITE, "v3_test_role_permission"}};
            auto roleGrantPermissionReply = co_await _etcd->roleGrantPermission(roleGrantPermissionReq);
            if(!roleGrantPermissionReply) {
                throw std::runtime_error("role grant permission");
            }

            roleGetReply = co_await _etcd->roleGet(roleGetReq);
            if(!roleGetReply) {
                throw std::runtime_error("roleGetReply1");
            }

            if(std::find_if(roleGetReply->perm.begin(), roleGetReply->perm.end(), [](Etcdv3::v1::AuthPermission const &permission){
                return permission.key == "v3_test_role_permission" && permission.permType == Etcdv3::v1::EtcdAuthPermissionType::WRITE;
            }) == roleGetReply->perm.end()) {
                throw std::runtime_error("permission not in role's permission list");
            }

            Etcdv3::v1::AuthRoleRevokePermissionRequest roleRevokePermissionReq{"v3_test_role", "v3_test_role_permission"};
            auto roleRevokeRoleReply = co_await _etcd->roleRevokePermission(roleRevokePermissionReq);
            if(!roleRevokeRoleReply) {
                throw std::runtime_error("role revoke permission");
            }

            roleGetReply = co_await _etcd->roleGet(roleGetReq);
            if(!roleGetReply) {
                throw std::runtime_error("roleGetReply2");
            }

            if(std::find_if(roleGetReply->perm.begin(), roleGetReply->perm.end(), [](Etcdv3::v1::AuthPermission const &permission){
                return permission.key == "v3_test_role_permission" && permission.permType == Etcdv3::v1::EtcdAuthPermissionType::WRITE;
            }) != roleGetReply->perm.end()) {
                throw std::runtime_error("permission still in role's permission list");
            }

            Etcdv3::v1::AuthRoleDeleteRequest roleDelReq{"v3_test_role"};
            auto roleDelReply = co_await _etcd->roleDelete(roleDelReq);
            if(!roleDelReply) {
                throw std::runtime_error("roleDel");
            }
        }

        Task<void> compact_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            int64_t revision{};
            Etcdv3::v1::EtcdPutRequest putReq{"v3_compaction_key", "compaction_value", tl::nullopt, tl::nullopt, tl::nullopt, tl::nullopt};

            auto putReply = co_await _etcd->put(putReq);
            if (!putReply) {
                throw std::runtime_error("put");
            }

            revision = putReply->header.revision;

            Etcdv3::v1::EtcdCompactionRequest compactionRequest{revision, tl::nullopt};

            auto compactionReply = co_await _etcd->compact(compactionRequest);
            if (!compactionReply) {
                throw std::runtime_error("compactionReply");
            }
        }

        Etcdv3::v1::IEtcd *_etcd;
        ILogger *_logger{};
        Version _v{};
    };
}
