#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/etcd/IEtcdV2.h>
#include <ichor/events/RunFunctionEvent.h>
#include <thread>

namespace Ichor {
    struct Etcdv2UsingService final : public AdvancedService<Etcdv2UsingService> {
        Etcdv2UsingService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
            reg.registerDependency<Etcdv2::v1::IEtcd>(this, DependencyFlags::REQUIRED);
        }

        Task<tl::expected<void, Ichor::StartError>> start() final {
            uint64_t modifiedIndex{};
            // standard create and get test
            {
                auto putReply = co_await _etcd->put("test_key", "test_value");
                if (!putReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!putReply.value().action || putReply.value().action != "set") {
                    fmt::println("Incorrect put action");
                    std::terminate();
                }

                if (!putReply.value().node || putReply.value().node->value != "test_value") {
                    fmt::println("Incorrect put node value");
                    std::terminate();
                }

                modifiedIndex = putReply.value().node->modifiedIndex;

                if(!putReply->x_etcd_index || putReply->x_etcd_index != modifiedIndex) {
                    fmt::println("Incorrect put x etcd index");
                    std::terminate();
                }

                auto getReply = co_await _etcd->get("test_key");
                if(!getReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!getReply.value().action || getReply.value().action != "get") {
                    fmt::println("Incorrect get action");
                    std::terminate();
                }

                if (!getReply.value().node || getReply.value().node->value != "test_value") {
                    fmt::println("Incorrect get node value");
                    std::terminate();
                }

                if (getReply.value().node->modifiedIndex != modifiedIndex) {
                    fmt::println("Incorrect get node modifiedIndex");
                    std::terminate();
                }
            }
            // Standard delete and get non-existing key test
            {
                auto delReply = co_await _etcd->del("test_key", false, false);
                if (!delReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!delReply.value().action || delReply.value().action != "delete") {
                    fmt::println("Incorrect del action");
                    std::terminate();
                }

                if (!delReply.value().node) {
                    fmt::println("Incorrect del node");
                    std::terminate();
                }

                if (delReply.value().node->value) {
                    fmt::println("Incorrect del node value");
                    std::terminate();
                }

                if (delReply.value().errorCode) {
                    fmt::println("Incorrect del errorCode");
                    std::terminate();
                }

                if (delReply.value().node->modifiedIndex != modifiedIndex + 1) {
                    fmt::println("Incorrect del node modifiedIndex");
                    std::terminate();
                }

                auto getReply = co_await _etcd->get("test_key");
                if(!getReply || getReply->errorCode != Etcdv2::v1::EtcdErrorCodes::KEY_DOES_NOT_EXIST) {
                    fmt::println("Incorrect get errorCode");
                    std::terminate();
                }
            }
            // Compare and swap previous value test
            {
                auto putReply = co_await _etcd->put("test_key", "test_value");
                if (!putReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!putReply.value().node || putReply.value().node->value != "test_value") {
                    fmt::println("Incorrect put node value");
                    std::terminate();
                }

                auto putWrongCasReply = co_await _etcd->put("test_key", "wrong_value", "wrong_previous_value", {}, {}, {}, false, false, false);
                if (!putWrongCasReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!putWrongCasReply.value().errorCode || putWrongCasReply.value().errorCode != Etcdv2::v1::EtcdErrorCodes::COMPARE_AND_SWAP_FAILED) {
                    fmt::println("Incorrect put wrong cas node errorCode");
                    std::terminate();
                }

                auto putCasReply = co_await _etcd->put("test_key", "new_value", "test_value", {}, {}, {}, false, false, false);
                if (!putCasReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (putCasReply.value().errorCode) {
                    fmt::println("Incorrect put cas errorCode");
                    std::terminate();
                }

                auto getReply = co_await _etcd->get("test_key");
                if(!getReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!getReply.value().node || getReply.value().node->value != "new_value") {
                    fmt::println("Incorrect get node value");
                    std::terminate();
                }

                auto delReply = co_await _etcd->del("test_key", false, false);
                if (!delReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (delReply.value().errorCode) {
                    fmt::println("Incorrect del errorCode");
                    std::terminate();
                }
            }
            // Compare and swap previous index test
            {
                auto putReply = co_await _etcd->put("test_key", "test_value");
                if (!putReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!putReply.value().node || putReply.value().node->value != "test_value") {
                    fmt::println("Incorrect put node value");
                    std::terminate();
                }
                uint64_t previous_index = putReply->node->modifiedIndex;

                auto putWrongCasReply = co_await _etcd->put("test_key", "wrong_value", {}, previous_index + 1, {}, {}, false, false, false);
                if (!putWrongCasReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!putWrongCasReply.value().errorCode || putWrongCasReply.value().errorCode != Etcdv2::v1::EtcdErrorCodes::COMPARE_AND_SWAP_FAILED) {
                    fmt::println("Incorrect put wrong cas node errorCode");
                    std::terminate();
                }

                auto putCasReply = co_await _etcd->put("test_key", "new_value", {}, previous_index, {}, {}, false, false, false);
                if (!putCasReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (putCasReply.value().errorCode) {
                    fmt::println("Incorrect put cas errorCode");
                    std::terminate();
                }

                auto getReply = co_await _etcd->get("test_key");
                if(!getReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!getReply.value().node || getReply.value().node->value != "new_value") {
                    fmt::println("Incorrect get node value");
                    std::terminate();
                }

                auto delReply = co_await _etcd->del("test_key", false, false);
                if (!delReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (delReply.value().errorCode) {
                    fmt::println("Incorrect del errorCode");
                    std::terminate();
                }
            }
            // previous exists tests
            {
                auto putWrongCasReply = co_await _etcd->put("test_key", "test_value", {}, {}, true, {}, false, false, false);
                if (!putWrongCasReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!putWrongCasReply.value().errorCode || putWrongCasReply.value().errorCode != Etcdv2::v1::EtcdErrorCodes::KEY_DOES_NOT_EXIST) {
                    fmt::println("Incorrect put wrong cas node errorCode");
                    std::terminate();
                }

                auto putCasReply = co_await _etcd->put("test_key", "test_value", {}, {}, false, {}, false, false, false);
                if (!putCasReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (putCasReply.value().errorCode) {
                    fmt::println("Incorrect put cas errorCode");
                    std::terminate();
                }

                putWrongCasReply = co_await _etcd->put("test_key", "test_value", {}, {}, false, {}, false, false, false);
                if (!putWrongCasReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!putWrongCasReply.value().errorCode || putWrongCasReply.value().errorCode != Etcdv2::v1::EtcdErrorCodes::KEY_ALREADY_EXISTS) {
                    fmt::println("Incorrect put wrong cas node errorCode");
                    std::terminate();
                }

                putCasReply = co_await _etcd->put("test_key", "new_value", {}, {}, true, {}, false, false, false);
                if (!putCasReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (putCasReply.value().errorCode) {
                    fmt::println("Incorrect put cas errorCode");
                    std::terminate();
                }

                auto getReply = co_await _etcd->get("test_key");
                if(!getReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!getReply.value().node || getReply.value().node->value != "new_value") {
                    fmt::println("Incorrect get node value");
                    std::terminate();
                }

                auto delReply = co_await _etcd->del("test_key", false, false);
                if (!delReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (delReply.value().errorCode) {
                    fmt::println("Incorrect del errorCode");
                    std::terminate();
                }
            }
            // Recursive directory test
            {
                co_await _etcd->del("test_key_dir", true, true);
                auto putReply = co_await _etcd->put("test_key_dir", "root", {}, {}, {}, {}, false, true, false);
                if (!putReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!putReply.value().node || putReply.value().node->value) {
                    fmt::println("Incorrect put dir node value1");
                    std::terminate();
                }

                if (!putReply.value().node->dir || !*putReply.value().node->dir) {
                    fmt::println("Incorrect put dir node dir");
                    std::terminate();
                }

                putReply = co_await _etcd->put("test_key_dir/one", "one_value", {}, {}, {}, {}, false, false, false);
                if (!putReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!putReply.value().node || putReply.value().node->value != "one_value") {
                    fmt::println("Incorrect put node value");
                    std::terminate();
                }

                putReply = co_await _etcd->put("test_key_dir/two", "two_value", {}, {}, {}, {}, false, false, false);
                if (!putReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!putReply.value().node || putReply.value().node->value != "two_value") {
                    fmt::println("Incorrect put node value");
                    std::terminate();
                }

                putReply = co_await _etcd->put("test_key_dir/a", "a_value", {}, {}, {}, {}, false, false, false);
                if (!putReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!putReply.value().node || putReply.value().node->value != "a_value") {
                    fmt::println("Incorrect put node value");
                    std::terminate();
                }

                auto getReply = co_await _etcd->get("test_key_dir", true, false, false, {});
                if(!getReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!getReply.value().node || getReply.value().node->value) {
                    fmt::println("Incorrect get dir node value");
                    std::terminate();
                }

                if (!getReply.value().node->dir || !*getReply.value().node->dir) {
                    fmt::println("Incorrect get dir node dir");
                    std::terminate();
                }

                if (!getReply.value().node->nodes || getReply.value().node->nodes->empty()) {
                    fmt::println("Incorrect get node nodes");
                    std::terminate();
                }

                bool found_a{};
                bool found_one{};
                bool found_two{};
                for(auto &node : *getReply.value().node->nodes) {
                    if(node.key == "/test_key_dir/a") {
                        if(!node.value || node.value != "a_value") {
                            fmt::println("Incorrect a_value");
                            std::terminate();
                        }
                        found_a = true;
                    }
                    if(node.key == "/test_key_dir/one") {
                        if(!node.value || node.value != "one_value") {
                            fmt::println("Incorrect one_value");
                            std::terminate();
                        }
                        found_one = true;
                    }
                    if(node.key == "/test_key_dir/two") {
                        if(!node.value || node.value != "two_value") {
                            fmt::println("Incorrect two_value");
                            std::terminate();
                        }
                        found_two = true;
                    }
                }
                if(!found_a) {
                    fmt::println("Did not find key a");
                    std::terminate();
                }
                if(!found_one) {
                    fmt::println("Did not find key one");
                    std::terminate();
                }
                if(!found_two) {
                    fmt::println("Did not find key two");
                    std::terminate();
                }

                getReply = co_await _etcd->get("test_key_dir", true, true, false, {});
                if(!getReply) {
                    fmt::println("");
                    std::terminate();
                }

                if((*getReply.value().node->nodes)[0].key != "/test_key_dir/a") {
                    fmt::println("Incorrect sorted one_value order");
                    std::terminate();
                }

                if((*getReply.value().node->nodes)[1].key != "/test_key_dir/one") {
                    fmt::println("Incorrect sorted one_value order");
                    std::terminate();
                }

                if((*getReply.value().node->nodes)[2].key != "/test_key_dir/two") {
                    fmt::println("Incorrect sorted two_value order");
                    std::terminate();
                }

                auto delReply = co_await _etcd->del("test_key_dir", true, true);
                if (!delReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (delReply.value().errorCode) {
                    fmt::println("Incorrect del errorCode");
                    std::terminate();
                }
            }
            // Recursive in-order directory test
            {
                auto putReply = co_await _etcd->put("test_key_dir", "root", {}, {}, {}, {}, false, false, true);
                if (!putReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!putReply.value().node || putReply.value().node->value != "root") {
                    fmt::println("Incorrect put dir node value");
                    std::terminate();
                }

                putReply = co_await _etcd->put("test_key_dir", "one_value", {}, {}, {}, {}, false, false, true);
                if (!putReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!putReply.value().node || putReply.value().node->value != "one_value") {
                    fmt::println("Incorrect put node value");
                    std::terminate();
                }

                putReply = co_await _etcd->put("test_key_dir", "two_value", {}, {}, {}, {}, false, false, true);
                if (!putReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!putReply.value().node || putReply.value().node->value != "two_value") {
                    fmt::println("Incorrect put node value");
                    std::terminate();
                }

                putReply = co_await _etcd->put("test_key_dir", "a_value", {}, {}, {}, {}, false, false, true);
                if (!putReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!putReply.value().node || putReply.value().node->value != "a_value") {
                    fmt::println("Incorrect put node value");
                    std::terminate();
                }

                auto getReply = co_await _etcd->get("test_key_dir", true, true, false, {});
                if(!getReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!getReply.value().node || getReply.value().node->value) {
                    fmt::println("Incorrect get dir node value");
                    std::terminate();
                }

                if (!getReply.value().node->dir || !*getReply.value().node->dir) {
                    fmt::println("Incorrect get dir node dir");
                    std::terminate();
                }

                if (!getReply.value().node->nodes || getReply.value().node->nodes->empty()) {
                    fmt::println("Incorrect get node nodes");
                    std::terminate();
                }

                if((*getReply.value().node->nodes)[0].value != "root") {
                    fmt::println("Incorrect root order");
                    std::terminate();
                }

                if((*getReply.value().node->nodes)[1].value != "one_value") {
                    fmt::println("Incorrect one_value order");
                    std::terminate();
                }

                if((*getReply.value().node->nodes)[2].value != "two_value") {
                    fmt::println("Incorrect two_value order");
                    std::terminate();
                }

                if((*getReply.value().node->nodes)[3].value != "a_value") {
                    fmt::println("Incorrect a_value order");
                    std::terminate();
                }

                auto delReply = co_await _etcd->del("test_key_dir", true, true);
                if (!delReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (delReply.value().errorCode) {
                    fmt::println("Incorrect del errorCode");
                    std::terminate();
                }
            }
            // TTL test
            {
                auto putReply = co_await _etcd->put("ttl_key", "value", {}, {}, {}, 1u, false, false, false);
                if (!putReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!putReply.value().node || putReply.value().node->value != "value") {
                    fmt::println("Incorrect put dir node value");
                    std::terminate();
                }

                if (putReply.value().node->ttl != 1) {
                    fmt::println("Incorrect put dir node ttl");
                    std::terminate();
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1500));

                auto getReply = co_await _etcd->get("ttl_key", false, false, false, {});
                if(!getReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!getReply.value().errorCode || getReply.value().errorCode != Etcdv2::v1::EtcdErrorCodes::KEY_DOES_NOT_EXIST) {
                    fmt::println("Incorrect ttl node errorCode");
                    std::terminate();
                }
            }
            // Watch test
            {
                {
                    auto putReply = co_await _etcd->put("watch_key", "value", {}, {}, {}, {}, false, false, false);
                    if (!putReply) {
                        fmt::println("");
                        std::terminate();
                    }

                    if (!putReply.value().node || putReply.value().node->value != "value") {
                        fmt::println("Incorrect put dir node value");
                        std::terminate();
                    }
                }

                auto start = std::chrono::steady_clock::now();
                GetThreadLocalEventQueue().pushEvent<RunFunctionEventAsync>(0, [this, start]() -> AsyncGenerator<IchorBehaviour> {
                    auto now = std::chrono::steady_clock::now();
                    while(now - start < std::chrono::milliseconds(250)) {
                        co_yield IchorBehaviour::DONE;
                        now = std::chrono::steady_clock::now();
                    }

                    auto putReply = co_await _etcd->put("watch_key", "new_value", {}, {}, {}, {}, false, false, false);
                    if (!putReply) {
                        fmt::println("");
                        std::terminate();
                    }

                    if (!putReply.value().node || putReply.value().node->value != "new_value") {
                        fmt::println("Incorrect put dir node value");
                        std::terminate();
                    }

                    co_return {};
                });

                auto getReply = co_await _etcd->get("watch_key", false, false, true, {});
                if(!getReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!getReply.value().node || getReply.value().node->value != "new_value") {
                    fmt::println("Incorrect get node value");
                    std::terminate();
                }
            }
            // Health test
            {
                auto healthReply = co_await _etcd->health();
                if (!healthReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (!healthReply.value()) {
                    fmt::println("Incorrect health value");
                    std::terminate();
                }
            }
            // Version test
            {
                auto versionReply = co_await _etcd->version();
                if (!versionReply) {
                    fmt::println("");
                    std::terminate();
                }

                if (versionReply.value().etcdcluster.empty()) {
                    fmt::println("Incorrect etcdcluster value");
                    std::terminate();
                }

                if (versionReply.value().etcdserver.empty()) {
                    fmt::println("Incorrect etcdcluster value");
                    std::terminate();
                }
            }
            // Authentication test
            {
                auto authReply = co_await _etcd->authStatus();
                if (!authReply) {
                    fmt::println("missing authReply");
                    std::terminate();
                }

                if(authReply.value()) {
                    _etcd->setAuthentication("root", "root");

                    auto disableAuthReply = co_await _etcd->disableAuth();
                    if (!disableAuthReply || !disableAuthReply.value()) {
                        fmt::println("These tests require that auth is disabled before starting the tests, or that the tests can disable it automatically using root:root");
                        std::terminate();
                    }

                }

                auto usersReply = co_await _etcd->getUsers();
                if (!usersReply) {
                    fmt::println("missing usersReply");
                    std::terminate();
                }

                if(!usersReply.value().users || std::find_if(usersReply.value().users->begin(), usersReply.value().users->end(), [](Etcdv2::v1::EtcdUserReply const &r) {
                    return r.user == "root";
                }) == usersReply.value().users->end()) {
                    auto createUserReply = co_await _etcd->createUser("root", "root");
                    if (!createUserReply) {
                        fmt::println("missing createUserReply");
                        std::terminate();
                    }
                }

                _etcd->setAuthentication("root", "root");
                if(_etcd->getAuthenticationUser() != "root") {
                    fmt::println("Incorrect auth user");
                    std::terminate();
                }

                auto enableAuthReply = co_await _etcd->enableAuth();
                if (!enableAuthReply) {
                    fmt::println("missing enableAuthReply");
                    std::terminate();
                }

                if(!enableAuthReply.value()) {
                    fmt::println("Could not enable auth");
                    std::terminate();
                }

                authReply = co_await _etcd->authStatus();
                if (!authReply) {
                    fmt::println("missing authReply");
                    std::terminate();
                }

                if(!authReply.value()) {
                    fmt::println("Expected Auth enabled");
                    std::terminate();
                }

                auto disableAuthReply = co_await _etcd->disableAuth();
                if (!disableAuthReply) {
                    fmt::println("missing disableAuthReply");
                    std::terminate();
                }

                if(!disableAuthReply.value()) {
                    fmt::println("Could not disable auth");
                    std::terminate();
                }
            }
            // Create / delete user tests
            {
                auto usersReply = co_await _etcd->getUsers();
                if (!usersReply) {
                    fmt::println("missing usersReply");
                    std::terminate();
                }

                if(!usersReply.value().users || std::find_if(usersReply.value().users->begin(), usersReply.value().users->end(), [](Etcdv2::v1::EtcdUserReply const &r) {
                    return r.user == "test_user";
                }) != usersReply.value().users->end()) {
                    fmt::println("test_user already exists");
                    std::terminate();
                }

                auto createUserReply = co_await _etcd->createUser("test_user", "test_user");
                if (!createUserReply) {
                    fmt::println("missing createUserReply");
                    std::terminate();
                }

                usersReply = co_await _etcd->getUsers();
                if (!usersReply) {
                    fmt::println("missing usersReply");
                    std::terminate();
                }

                if(!usersReply.value().users || std::find_if(usersReply.value().users->begin(), usersReply.value().users->end(), [](Etcdv2::v1::EtcdUserReply const &r) {
                    return r.user == "test_user";
                }) == usersReply.value().users->end()) {
                    fmt::println("test_user not created successfully");
                    std::terminate();
                }

                // forced to declare as a variable due to internal compiler errors in gcc <= 12.3.0
                std::vector<std::string> roles{"test_role"};
                auto grantUserReply = co_await _etcd->grantUserRoles("test_user", roles);
                if (!grantUserReply) {
                    fmt::println("missing grantUserReply");
                    std::terminate();
                }

                auto revokeUserReply = co_await _etcd->revokeUserRoles("test_user", roles);
                if (!revokeUserReply) {
                    fmt::println("missing revokeUserReply");
                    std::terminate();
                }

                auto updatePassReply = co_await _etcd->updateUserPassword("test_user", "test_role2");
                if (!updatePassReply) {
                    fmt::println("missing updatePassReply");
                    std::terminate();
                }

                auto deleteUserReply = co_await _etcd->deleteUser("test_user");
                if (!deleteUserReply) {
                    fmt::println("missing deleteUserReply");
                    std::terminate();
                }

                usersReply = co_await _etcd->getUsers();
                if (!usersReply) {
                    fmt::println("missing usersReply");
                    std::terminate();
                }

                if(!usersReply.value().users || std::find_if(usersReply.value().users->begin(), usersReply.value().users->end(), [](Etcdv2::v1::EtcdUserReply const &r) {
                    return r.user == "test_user";
                }) != usersReply.value().users->end()) {
                    fmt::println("test_user not deleted successfully");
                    std::terminate();
                }
            }
            // Create / delete role tests
            {
                auto rolesReply = co_await _etcd->getRoles();
                if (!rolesReply) {
                    fmt::println("missing rolesReply");
                    std::terminate();
                }

                if(std::find_if(rolesReply.value().roles.begin(), rolesReply.value().roles.end(), [](Etcdv2::v1::EtcdRoleReply const &r) {
                    return r.role == "test_role";
                }) != rolesReply.value().roles.end()) {
                    fmt::println("test_role already exists");
                    std::terminate();
                }

                // forced to declare as a variable due to internal compiler errors in gcc <= 12.3.0
                std::vector<std::string> read_permissions{"read_perm"};
                std::vector<std::string> write_permissions{"write_perm"};
                auto createRoleReply = co_await _etcd->createRole("test_role", read_permissions, write_permissions);
                if (!createRoleReply) {
                    fmt::print("missing createRoleReply {}\n", createRoleReply.error());
                    fmt::println("missing createRoleReply");
                    std::terminate();
                }

                rolesReply = co_await _etcd->getRoles();
                if (!rolesReply) {
                    fmt::println("missing rolesReply");
                    std::terminate();
                }

                if(std::find_if(rolesReply.value().roles.begin(), rolesReply.value().roles.end(), [](Etcdv2::v1::EtcdRoleReply const &r) {
                    return r.role == "test_role";
                }) == rolesReply.value().roles.end()) {
                    fmt::println("test_role not created successfully");
                    std::terminate();
                }

                read_permissions.clear();
                read_permissions.push_back("read_perm2");
                auto grantRoleReply = co_await _etcd->grantRolePermissions("test_role", read_permissions, {});
                if (!grantRoleReply) {
                    fmt::print("missing grantRoleReply {}\n", grantRoleReply.error());
                    fmt::println("missing grantRoleReply");
                    std::terminate();
                }

                auto revokeRoleReply = co_await _etcd->revokeRolePermissions("test_role", read_permissions, {});
                if (!revokeRoleReply) {
                    fmt::print("missing revokeRoleReply {}\n", revokeRoleReply.error());
                    fmt::println("missing revokeRoleReply");
                    std::terminate();
                }

                auto deleteRoleReply = co_await _etcd->deleteRole("test_role");
                if (!deleteRoleReply) {
                    fmt::println("missing deleteRoleReply");
                    std::terminate();
                }

                rolesReply = co_await _etcd->getRoles();
                if (!rolesReply) {
                    fmt::println("missing rolesReply");
                    std::terminate();
                }

                if(std::find_if(rolesReply.value().roles.begin(), rolesReply.value().roles.end(), [](Etcdv2::v1::EtcdRoleReply const &r) {
                    return r.role == "test_role";
                }) != rolesReply.value().roles.end()) {
                    fmt::println("test_role not deleted successfully");
                    std::terminate();
                }
            }

            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());

            co_return {};
        }

        void addDependencyInstance(Etcdv2::v1::IEtcd &Etcd, IService&) {
            _etcd = &Etcd;
        }

        void removeDependencyInstance(Etcdv2::v1::IEtcd&, IService&) {
            _etcd = nullptr;
        }

        Etcdv2::v1::IEtcd *_etcd;
    };
}
