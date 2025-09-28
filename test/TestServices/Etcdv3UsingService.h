#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/etcd/IEtcdV3.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/ServiceExecutionScope.h>

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

        void addDependencyInstance(Ichor::ScopedServiceProxy<Etcdv3::v1::IEtcd*> Etcd, IService&) {
            _etcd = std::move(Etcd);
        }

        void removeDependencyInstance(Ichor::ScopedServiceProxy<Etcdv3::v1::IEtcd*>, IService&) {
            _etcd.reset();
        }

        void addDependencyInstance(Ichor::ScopedServiceProxy<ILogger*> logger, IService&) {
            _logger = std::move(logger);
        }

        void removeDependencyInstance(Ichor::ScopedServiceProxy<ILogger*>, IService&) {
            _logger.reset();
        }

        Task<void> put_get_delete_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            int64_t revision{};
            Etcdv3::v1::EtcdPutRequest putReq{"v3_test_key", "test_value", 0, tl::nullopt, tl::nullopt, tl::nullopt};

            auto putReply = co_await _etcd->put(putReq);
            if (!putReply) {
                fmt::println("put");
                std::terminate();
            }
            revision = putReply->header.revision;

            Etcdv3::v1::EtcdRangeRequest rangeReq{.key = "v3_test_key"};
            auto rangeReply = co_await _etcd->range(rangeReq);
            if (!rangeReply) {
                fmt::println("range");
                std::terminate();
            }

            if ((*rangeReply).kvs.size() != 1) {
                fmt::println("range size");
                std::terminate();
            }

            if ((*rangeReply).kvs[0].key != "v3_test_key") {
                fmt::println("range key");
                std::terminate();
            }

            if ((*rangeReply).kvs[0].value != "test_value") {
                fmt::println("range value");
                std::terminate();
            }

            Etcdv3::v1::EtcdDeleteRangeRequest deleteReq{.key = "v3_test_key"};

            auto deleteReply = co_await _etcd->deleteRange(deleteReq);
            if (!deleteReply) {
                fmt::println("delete");
                std::terminate();
            }

            if(deleteReply->deleted != 1) {
                fmt::println("delete deleted != 1");
                std::terminate();
            }

            if(deleteReply->header.revision != revision + 1) {
                fmt::println("revision");
                std::terminate();
            }
        }

        Task<void> txn_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            int64_t revision{};
            Etcdv3::v1::EtcdPutRequest putReq{"v3_txn_key", "txn_value", tl::nullopt, tl::nullopt, tl::nullopt, tl::nullopt};

            auto putReply = co_await _etcd->put(putReq);
            if (!putReply) {
                fmt::println("put");
                std::terminate();
            }

            Etcdv3::v1::EtcdRangeRequest rangeReq{.key = "v3_txn_key"};
            auto rangeReply = co_await _etcd->range(rangeReq);
            if (!rangeReply) {
                fmt::println("range");
                std::terminate();
            }
            if (rangeReply->kvs.size() != 1) {
                fmt::println("range size");
                std::terminate();
            }

            revision = rangeReply->kvs[0].create_revision;

            Etcdv3::v1::EtcdTxnRequest txnReq{};
            txnReq.compare.emplace_back(Etcdv3::v1::EtcdCompare{.target = Etcdv3::v1::EtcdCompareTarget::CREATE,.key = "v3_txn_key", .create_revision = revision});
            txnReq.success.emplace_back(Etcdv3::v1::EtcdRequestOp{.request_range = Etcdv3::v1::EtcdRangeRequest{.key = "v3_txn_key"}});
            auto txnReply = co_await _etcd->txn(txnReq);
            if (!txnReply) {
                fmt::println("txn");
                std::terminate();
            }

            if(!txnReply->succeeded) {
                fmt::println("txn succeeded false");
                std::terminate();
            }

            if(txnReply->responses.size() != 1) {
                fmt::println("txn responses size != 1");
                std::terminate();
            }
        }

        Task<void> leases_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            Etcdv3::v1::LeaseGrantRequest grantReq{100, 101};

            auto grantReply = co_await _etcd->leaseGrant(grantReq);
            if (!grantReply) {
                fmt::println("grant");
                std::terminate();
            }

            Etcdv3::v1::LeaseKeepAliveRequest keepAliveReq{101};
            auto keepAliveReply = co_await _etcd->leaseKeepAlive(keepAliveReq);
            if (!keepAliveReply) {
                fmt::println("keepAlive");
                std::terminate();
            }
            if(keepAliveReply->result.id != 101) {
                fmt::println("keepAlive id != 101");
                std::terminate();
            }
            if(keepAliveReply->result.ttl_in_seconds <= 90) {
                fmt::println("keepAlive ttl_in_seconds <= 90");
                std::terminate();
            }

            Etcdv3::v1::EtcdPutRequest putReq{"v3_lease_key", "lease_value", 101, tl::nullopt, tl::nullopt, tl::nullopt};
            auto putReply = co_await _etcd->put(putReq);
            if (!putReply) {
                fmt::println("put");
                std::terminate();
            }

            Etcdv3::v1::LeaseTimeToLiveRequest ttlReq{101, true};
            auto ttlReply = co_await _etcd->leaseTimeToLive(ttlReq);
            if (!ttlReply) {
                fmt::println("ttl");
                std::terminate();
            }
            if(ttlReply->id != 101) {
                fmt::println("ttl id != 101");
                std::terminate();
            }
            if(ttlReply->ttl_in_seconds <= 90) {
                fmt::println("ttl ttl_in_seconds <= 90");
                std::terminate();
            }
            if(ttlReply->granted_ttl != 100) {
                fmt::println("ttl granted_ttl != 100");
                std::terminate();
            }
            if(ttlReply->keys.size() != 1) {
                fmt::println("ttl keys.size() != 1");
                std::terminate();
            }
            if(ttlReply->keys[0] != "v3_lease_key") {
                fmt::println("ttl keys[0] != v3_lease_key");
                std::terminate();
            }

            if(_v >= Version{3, 3, 0}) {
                Etcdv3::v1::LeaseLeasesRequest leasesReq{};
                auto leasesReply = co_await _etcd->leaseLeases(leasesReq);
                if (!leasesReply) {
                    fmt::println("leases");
                    std::terminate();
                }
                if (leasesReply->leases.size() != 1) {
                    fmt::println("leases leases.size() != 1");
                    std::terminate();
                }
                if (leasesReply->leases[0].id != 101) {
                    fmt::println("leases leases[0].id != 101");
                    std::terminate();
                }
            }

            Etcdv3::v1::LeaseRevokeRequest revokeReq{101};
            auto revokeReply = co_await _etcd->leaseRevoke(revokeReq);
            if(!revokeReply) {
                fmt::println("revoke");
                std::terminate();
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
                    fmt::println("userDel at init");
                    std::terminate();
                }
            }

            Etcdv3::v1::AuthUserAddRequest userAddReq{"v3_test_user", "v3_test_password", {}, {}};
            auto userAddReply = co_await _etcd->userAdd(userAddReq);
            if (!userAddReply) {
                fmt::println("userAdd");
                std::terminate();
            }

            Etcdv3::v1::AuthUserListRequest userListReq{};
            auto userListReply = co_await _etcd->userList(userListReq);
            if (!userListReply) {
                fmt::println("userList");
                std::terminate();
            }
            if(std::find(userListReply->users.begin(), userListReply->users.end(), "v3_test_user") == userListReply->users.end()) {
                fmt::println("v3_test_user not in list user");
                std::terminate();
            }
            if(std::find(userListReply->users.begin(), userListReply->users.end(), "root") == userListReply->users.end()) {
                userAddReq.name = "root";
                userAddReq.password = "root";
                userAddReply = co_await _etcd->userAdd(userAddReq);
                if (!userAddReply) {
                    fmt::println("userAdd root");
                    std::terminate();
                }

                Etcdv3::v1::AuthUserGrantRoleRequest userGrantRoleReq{"root", "root"};
                auto userGrantRoleReply = co_await _etcd->userGrantRole(userGrantRoleReq);
                if(!userGrantRoleReply) {
                    fmt::println("grant role root");
                    std::terminate();
                }
            }

            Etcdv3::v1::AuthUserGrantRoleRequest userGrantRoleReq{"v3_test_user", "root"};
            auto userGrantRoleReply = co_await _etcd->userGrantRole(userGrantRoleReq);
            if(!userGrantRoleReply) {
                fmt::println("grant role");
                std::terminate();
            }

            userGetReply = co_await _etcd->userGet(userGetReq);
            if(!userGetReply) {
                fmt::println("userGetReply");
                std::terminate();
            }

            if(std::find(userGetReply->roles.begin(), userGetReply->roles.end(), "root") == userGetReply->roles.end()) {
                fmt::println("root not in user's role list");
                std::terminate();
            }

            Etcdv3::v1::AuthEnableRequest authEnableReq{};
            auto authEnableReply = co_await _etcd->authEnable(authEnableReq);
            if(_v < Version{3, 3, 0} && authEnableReply) {
                fmt::println("auth enable");
                std::terminate();
            }
            if(_v >= Version{3, 3, 0} && !authEnableReply) {
                fmt::println("auth enable");
                std::terminate();
            }

            Etcdv3::v1::AuthenticateRequest userAuthReq{"v3_test_user", "v3_test_password"};
            auto userAuthReply = co_await _etcd->authenticate(userAuthReq);
            if(_v < Version{3, 3, 0} && userAuthReply) {
                fmt::println("authenticate");
                std::terminate();
            }
            if(_v >= Version{3, 3, 0} && !userAuthReply) {
                fmt::println("authenticate");
                std::terminate();
            }
            if(_v >= Version{3, 3, 0} && _etcd->getAuthenticationUser() != "v3_test_user") {
                fmt::println("authenticate user");
                std::terminate();
            }

            Etcdv3::v1::AuthDisableRequest authDisableReq{};
            auto authDisableReply = co_await _etcd->authDisable(authDisableReq);
            if(_v < Version{3, 3, 0} && authDisableReply) {
                fmt::println("auth disable");
                std::terminate();
            }
            if(_v >= Version{3, 3, 0} && !authDisableReply) {
                fmt::println("auth disable");
                std::terminate();
            }

            Etcdv3::v1::AuthUserChangePasswordRequest authChangePassReq{"v3_test_user", "v3_test_password2"};
            auto authChangePassReply = co_await _etcd->userChangePassword(authChangePassReq);
            if(!authChangePassReply) {
                fmt::println("auth change pass");
                std::terminate();
            }

            authEnableReply = co_await _etcd->authEnable(authEnableReq);
            if(_v < Version{3, 3, 0} && authEnableReply) {
                fmt::println("auth enable2");
                std::terminate();
            }
            if(_v >= Version{3, 3, 0} && !authEnableReply) {
                fmt::println("auth enable2");
                std::terminate();
            }

            userAuthReq.password = "v3_test_password2";
            userAuthReply = co_await _etcd->authenticate(userAuthReq);
            if(_v < Version{3, 3, 0} && userAuthReply) {
                fmt::println("authenticate2");
                std::terminate();
            }
            if(_v >= Version{3, 3, 0} && !userAuthReply) {
                fmt::println("authenticate2");
                std::terminate();
            }
            if(_v >= Version{3, 3, 0} && _etcd->getAuthenticationUser() != "v3_test_user") {
                fmt::println("authenticate user2");
                std::terminate();
            }

            authDisableReply = co_await _etcd->authDisable(authDisableReq);
            if(_v < Version{3, 3, 0} && authDisableReply) {
                fmt::println("auth disable2");
                std::terminate();
            }
            if(_v >= Version{3, 3, 0} && !authDisableReply) {
                fmt::println("auth disable2");
                std::terminate();
            }



            Etcdv3::v1::AuthUserRevokeRoleRequest userRevokeRoleReq{"v3_test_user", "root"};
            auto userRevokeRoleReply = co_await _etcd->userRevokeRole(userRevokeRoleReq);
            if(!userRevokeRoleReply) {
                fmt::println("revoke role");
                std::terminate();
            }

            userGetReply = co_await _etcd->userGet(userGetReq);
            if(!userGetReply) {
                fmt::println("userGetReply");
                std::terminate();
            }

            if(std::find(userGetReply->roles.begin(), userGetReply->roles.end(), "root") != userGetReply->roles.end()) {
                fmt::println("root still in user's role list");
                std::terminate();
            }

            Etcdv3::v1::AuthUserDeleteRequest userDelReq{"v3_test_user"};
            auto userDelReply = co_await _etcd->userDelete(userDelReq);
            if(!userDelReply) {
                fmt::println("userDel");
                std::terminate();
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
                    fmt::println("roleDel at init");
                    std::terminate();
                }
            }

            Etcdv3::v1::AuthRoleAddRequest roleAddReq{"v3_test_role"};
            auto roleAddReply = co_await _etcd->roleAdd(roleAddReq);
            if (!roleAddReply) {
                fmt::println("roleAdd");
                std::terminate();
            }

            Etcdv3::v1::AuthRoleListRequest roleListReq{};
            auto roleListReply = co_await _etcd->roleList(roleListReq);
            if (!roleListReply) {
                fmt::println("roleList");
                std::terminate();
            }
            if(std::find(roleListReply->roles.begin(), roleListReply->roles.end(), "v3_test_role") == roleListReply->roles.end()) {
                fmt::println("v3_test_role not in list role");
                std::terminate();
            }

            Etcdv3::v1::AuthRoleGrantPermissionRequest roleGrantPermissionReq{"v3_test_role", Etcdv3::v1::AuthPermission{Etcdv3::v1::EtcdAuthPermissionType::WRITE, "v3_test_role_permission"}};
            auto roleGrantPermissionReply = co_await _etcd->roleGrantPermission(roleGrantPermissionReq);
            if(!roleGrantPermissionReply) {
                fmt::println("role grant permission");
                std::terminate();
            }

            roleGetReply = co_await _etcd->roleGet(roleGetReq);
            if(!roleGetReply) {
                fmt::println("roleGetReply1");
                std::terminate();
            }

            if(std::find_if(roleGetReply->perm.begin(), roleGetReply->perm.end(), [](Etcdv3::v1::AuthPermission const &permission){
                return permission.key == "v3_test_role_permission" && permission.permType == Etcdv3::v1::EtcdAuthPermissionType::WRITE;
            }) == roleGetReply->perm.end()) {
                fmt::println("permission not in role's permission list");
                std::terminate();
            }

            Etcdv3::v1::AuthRoleRevokePermissionRequest roleRevokePermissionReq{"v3_test_role", "v3_test_role_permission"};
            auto roleRevokeRoleReply = co_await _etcd->roleRevokePermission(roleRevokePermissionReq);
            if(!roleRevokeRoleReply) {
                fmt::println("role revoke permission");
                std::terminate();
            }

            roleGetReply = co_await _etcd->roleGet(roleGetReq);
            if(!roleGetReply) {
                fmt::println("roleGetReply2");
                std::terminate();
            }

            if(std::find_if(roleGetReply->perm.begin(), roleGetReply->perm.end(), [](Etcdv3::v1::AuthPermission const &permission){
                return permission.key == "v3_test_role_permission" && permission.permType == Etcdv3::v1::EtcdAuthPermissionType::WRITE;
            }) != roleGetReply->perm.end()) {
                fmt::println("permission still in role's permission list");
                std::terminate();
            }

            Etcdv3::v1::AuthRoleDeleteRequest roleDelReq{"v3_test_role"};
            auto roleDelReply = co_await _etcd->roleDelete(roleDelReq);
            if(!roleDelReply) {
                fmt::println("roleDel");
                std::terminate();
            }
        }

        Task<void> compact_test() const {
            ICHOR_LOG_INFO(_logger, "running test");
            int64_t revision{};
            Etcdv3::v1::EtcdPutRequest putReq{"v3_compaction_key", "compaction_value", tl::nullopt, tl::nullopt, tl::nullopt, tl::nullopt};

            auto putReply = co_await _etcd->put(putReq);
            if (!putReply) {
                fmt::println("put");
                std::terminate();
            }

            revision = putReply->header.revision;

            Etcdv3::v1::EtcdCompactionRequest compactionRequest{revision, tl::nullopt};

            auto compactionReply = co_await _etcd->compact(compactionRequest);
            if (!compactionReply) {
                fmt::println("compactionReply");
                std::terminate();
            }
        }

        Ichor::ScopedServiceProxy<Etcdv3::v1::IEtcd*> _etcd ;
        Ichor::ScopedServiceProxy<ILogger*> _logger {};
        Version _v{};
    };
}
