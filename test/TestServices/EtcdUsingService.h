#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/etcd/IEtcd.h>
#include <ichor/events/RunFunctionEvent.h>

namespace Ichor {
    struct EtcdUsingService final : public AdvancedService<EtcdUsingService> {
        EtcdUsingService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
            reg.registerDependency<IEtcd>(this, true);
        }

        Task<tl::expected<void, Ichor::StartError>> start() final {
            uint64_t modifiedIndex{};
            // standard create and get test
            {
                auto putReply = co_await _etcd->put("test_key", "test_value");
                if (!putReply) {
                    throw std::runtime_error("");
                }

                if (!putReply.value().action || putReply.value().action != "set") {
                    throw std::runtime_error("Incorrect put action");
                }

                if (!putReply.value().node || putReply.value().node->value != "test_value") {
                    throw std::runtime_error("Incorrect put node value");
                }

                modifiedIndex = putReply.value().node->modifiedIndex;

                if(!putReply->x_etcd_index || putReply->x_etcd_index != modifiedIndex) {
                    throw std::runtime_error("Incorrect put x etcd index");
                }

                auto getReply = co_await _etcd->get("test_key");
                if(!getReply) {
                    throw std::runtime_error("");
                }

                if (!getReply.value().action || getReply.value().action != "get") {
                    throw std::runtime_error("Incorrect get action");
                }

                if (!getReply.value().node || getReply.value().node->value != "test_value") {
                    throw std::runtime_error("Incorrect get node value");
                }

                if (getReply.value().node->modifiedIndex != modifiedIndex) {
                    throw std::runtime_error("Incorrect get node modifiedIndex");
                }
            }
            // Standard delete and get non-existing key test
            {
                auto delReply = co_await _etcd->del("test_key", false, false);
                if (!delReply) {
                    throw std::runtime_error("");
                }

                if (!delReply.value().action || delReply.value().action != "delete") {
                    throw std::runtime_error("Incorrect del action");
                }

                if (!delReply.value().node) {
                    throw std::runtime_error("Incorrect del node");
                }

                if (delReply.value().node->value) {
                    throw std::runtime_error("Incorrect del node value");
                }

                if (delReply.value().errorCode) {
                    throw std::runtime_error("Incorrect del errorCode");
                }

                if (delReply.value().node->modifiedIndex != modifiedIndex + 1) {
                    throw std::runtime_error("Incorrect del node modifiedIndex");
                }

                auto getReply = co_await _etcd->get("test_key");
                if(!getReply || getReply->errorCode != EtcdErrorCodes::KEY_DOES_NOT_EXIST) {
                    throw std::runtime_error("Incorrect get errorCode");
                }
            }
            // Compare and swap previous value test
            {
                auto putReply = co_await _etcd->put("test_key", "test_value");
                if (!putReply) {
                    throw std::runtime_error("");
                }

                if (!putReply.value().node || putReply.value().node->value != "test_value") {
                    throw std::runtime_error("Incorrect put node value");
                }

                auto putWrongCasReply = co_await _etcd->put("test_key", "wrong_value", "wrong_previous_value", {}, {}, {}, false, false, false);
                if (!putWrongCasReply) {
                    throw std::runtime_error("");
                }

                if (!putWrongCasReply.value().errorCode || putWrongCasReply.value().errorCode != EtcdErrorCodes::COMPARE_AND_SWAP_FAILED) {
                    throw std::runtime_error("Incorrect put wrong cas node errorCode");
                }

                auto putCasReply = co_await _etcd->put("test_key", "new_value", "test_value", {}, {}, {}, false, false, false);
                if (!putCasReply) {
                    throw std::runtime_error("");
                }

                if (putCasReply.value().errorCode) {
                    throw std::runtime_error("Incorrect put cas errorCode");
                }

                auto getReply = co_await _etcd->get("test_key");
                if(!getReply) {
                    throw std::runtime_error("");
                }

                if (!getReply.value().node || getReply.value().node->value != "new_value") {
                    throw std::runtime_error("Incorrect get node value");
                }

                auto delReply = co_await _etcd->del("test_key", false, false);
                if (!delReply) {
                    throw std::runtime_error("");
                }

                if (delReply.value().errorCode) {
                    throw std::runtime_error("Incorrect del errorCode");
                }
            }
            // Compare and swap previous index test
            {
                auto putReply = co_await _etcd->put("test_key", "test_value");
                if (!putReply) {
                    throw std::runtime_error("");
                }

                if (!putReply.value().node || putReply.value().node->value != "test_value") {
                    throw std::runtime_error("Incorrect put node value");
                }
                uint64_t previous_index = putReply->node->modifiedIndex;

                auto putWrongCasReply = co_await _etcd->put("test_key", "wrong_value", {}, previous_index + 1, {}, {}, false, false, false);
                if (!putWrongCasReply) {
                    throw std::runtime_error("");
                }

                if (!putWrongCasReply.value().errorCode || putWrongCasReply.value().errorCode != EtcdErrorCodes::COMPARE_AND_SWAP_FAILED) {
                    throw std::runtime_error("Incorrect put wrong cas node errorCode");
                }

                auto putCasReply = co_await _etcd->put("test_key", "new_value", {}, previous_index, {}, {}, false, false, false);
                if (!putCasReply) {
                    throw std::runtime_error("");
                }

                if (putCasReply.value().errorCode) {
                    throw std::runtime_error("Incorrect put cas errorCode");
                }

                auto getReply = co_await _etcd->get("test_key");
                if(!getReply) {
                    throw std::runtime_error("");
                }

                if (!getReply.value().node || getReply.value().node->value != "new_value") {
                    throw std::runtime_error("Incorrect get node value");
                }

                auto delReply = co_await _etcd->del("test_key", false, false);
                if (!delReply) {
                    throw std::runtime_error("");
                }

                if (delReply.value().errorCode) {
                    throw std::runtime_error("Incorrect del errorCode");
                }
            }
            // previous exists tests
            {
                auto putWrongCasReply = co_await _etcd->put("test_key", "test_value", {}, {}, true, {}, false, false, false);
                if (!putWrongCasReply) {
                    throw std::runtime_error("");
                }

                if (!putWrongCasReply.value().errorCode || putWrongCasReply.value().errorCode != EtcdErrorCodes::KEY_DOES_NOT_EXIST) {
                    throw std::runtime_error("Incorrect put wrong cas node errorCode");
                }

                auto putCasReply = co_await _etcd->put("test_key", "test_value", {}, {}, false, {}, false, false, false);
                if (!putCasReply) {
                    throw std::runtime_error("");
                }

                if (putCasReply.value().errorCode) {
                    throw std::runtime_error("Incorrect put cas errorCode");
                }

                putWrongCasReply = co_await _etcd->put("test_key", "test_value", {}, {}, false, {}, false, false, false);
                if (!putWrongCasReply) {
                    throw std::runtime_error("");
                }

                if (!putWrongCasReply.value().errorCode || putWrongCasReply.value().errorCode != EtcdErrorCodes::KEY_ALREADY_EXISTS) {
                    throw std::runtime_error("Incorrect put wrong cas node errorCode");
                }

                putCasReply = co_await _etcd->put("test_key", "new_value", {}, {}, true, {}, false, false, false);
                if (!putCasReply) {
                    throw std::runtime_error("");
                }

                if (putCasReply.value().errorCode) {
                    throw std::runtime_error("Incorrect put cas errorCode");
                }

                auto getReply = co_await _etcd->get("test_key");
                if(!getReply) {
                    throw std::runtime_error("");
                }

                if (!getReply.value().node || getReply.value().node->value != "new_value") {
                    throw std::runtime_error("Incorrect get node value");
                }

                auto delReply = co_await _etcd->del("test_key", false, false);
                if (!delReply) {
                    throw std::runtime_error("");
                }

                if (delReply.value().errorCode) {
                    throw std::runtime_error("Incorrect del errorCode");
                }
            }
            // Recursive directory test
            {
                auto putReply = co_await _etcd->put("test_key_dir", "root", {}, {}, {}, {}, false, true, false);
                if (!putReply) {
                    throw std::runtime_error("");
                }

                if (!putReply.value().node || putReply.value().node->value) {
                    throw std::runtime_error("Incorrect put dir node value");
                }

                if (!putReply.value().node->dir || !*putReply.value().node->dir) {
                    throw std::runtime_error("Incorrect put dir node dir");
                }

                putReply = co_await _etcd->put("test_key_dir/one", "one_value", {}, {}, {}, {}, false, false, false);
                if (!putReply) {
                    throw std::runtime_error("");
                }

                if (!putReply.value().node || putReply.value().node->value != "one_value") {
                    throw std::runtime_error("Incorrect put node value");
                }

                putReply = co_await _etcd->put("test_key_dir/two", "two_value", {}, {}, {}, {}, false, false, false);
                if (!putReply) {
                    throw std::runtime_error("");
                }

                if (!putReply.value().node || putReply.value().node->value != "two_value") {
                    throw std::runtime_error("Incorrect put node value");
                }

                putReply = co_await _etcd->put("test_key_dir/a", "a_value", {}, {}, {}, {}, false, false, false);
                if (!putReply) {
                    throw std::runtime_error("");
                }

                if (!putReply.value().node || putReply.value().node->value != "a_value") {
                    throw std::runtime_error("Incorrect put node value");
                }

                auto getReply = co_await _etcd->get("test_key_dir", true, false, false, {});
                if(!getReply) {
                    throw std::runtime_error("");
                }

                if (!getReply.value().node || getReply.value().node->value) {
                    throw std::runtime_error("Incorrect get dir node value");
                }

                if (!getReply.value().node->dir || !*getReply.value().node->dir) {
                    throw std::runtime_error("Incorrect get dir node dir");
                }

                if (!getReply.value().node->nodes || getReply.value().node->nodes->empty()) {
                    throw std::runtime_error("Incorrect get node nodes");
                }

                bool found_a{};
                bool found_one{};
                bool found_two{};
                for(auto &node : *getReply.value().node->nodes) {
                    if(node.key == "/test_key_dir/a") {
                        if(!node.value || node.value != "a_value") {
                            throw std::runtime_error("Incorrect a_value");
                        }
                        found_a = true;
                    }
                    if(node.key == "/test_key_dir/one") {
                        if(!node.value || node.value != "one_value") {
                            throw std::runtime_error("Incorrect one_value");
                        }
                        found_one = true;
                    }
                    if(node.key == "/test_key_dir/two") {
                        if(!node.value || node.value != "two_value") {
                            throw std::runtime_error("Incorrect two_value");
                        }
                        found_two = true;
                    }
                }
                if(!found_a) {
                    throw std::runtime_error("Did not find key a");
                }
                if(!found_one) {
                    throw std::runtime_error("Did not find key one");
                }
                if(!found_two) {
                    throw std::runtime_error("Did not find key two");
                }

                getReply = co_await _etcd->get("test_key_dir", true, true, false, {});
                if(!getReply) {
                    throw std::runtime_error("");
                }

                if((*getReply.value().node->nodes)[0].key != "/test_key_dir/a") {
                    throw std::runtime_error("Incorrect sorted one_value order");
                }

                if((*getReply.value().node->nodes)[1].key != "/test_key_dir/one") {
                    throw std::runtime_error("Incorrect sorted one_value order");
                }

                if((*getReply.value().node->nodes)[2].key != "/test_key_dir/two") {
                    throw std::runtime_error("Incorrect sorted two_value order");
                }

                auto delReply = co_await _etcd->del("test_key_dir", true, true);
                if (!delReply) {
                    throw std::runtime_error("");
                }

                if (delReply.value().errorCode) {
                    throw std::runtime_error("Incorrect del errorCode");
                }
            }
            // Recursive in-order directory test
            {
                auto putReply = co_await _etcd->put("test_key_dir", "root", {}, {}, {}, {}, false, false, true);
                if (!putReply) {
                    throw std::runtime_error("");
                }

                if (!putReply.value().node || putReply.value().node->value != "root") {
                    throw std::runtime_error("Incorrect put dir node value");
                }

                putReply = co_await _etcd->put("test_key_dir", "one_value", {}, {}, {}, {}, false, false, true);
                if (!putReply) {
                    throw std::runtime_error("");
                }

                if (!putReply.value().node || putReply.value().node->value != "one_value") {
                    throw std::runtime_error("Incorrect put node value");
                }

                putReply = co_await _etcd->put("test_key_dir", "two_value", {}, {}, {}, {}, false, false, true);
                if (!putReply) {
                    throw std::runtime_error("");
                }

                if (!putReply.value().node || putReply.value().node->value != "two_value") {
                    throw std::runtime_error("Incorrect put node value");
                }

                putReply = co_await _etcd->put("test_key_dir", "a_value", {}, {}, {}, {}, false, false, true);
                if (!putReply) {
                    throw std::runtime_error("");
                }

                if (!putReply.value().node || putReply.value().node->value != "a_value") {
                    throw std::runtime_error("Incorrect put node value");
                }

                auto getReply = co_await _etcd->get("test_key_dir", true, true, false, {});
                if(!getReply) {
                    throw std::runtime_error("");
                }

                if (!getReply.value().node || getReply.value().node->value) {
                    throw std::runtime_error("Incorrect get dir node value");
                }

                if (!getReply.value().node->dir || !*getReply.value().node->dir) {
                    throw std::runtime_error("Incorrect get dir node dir");
                }

                if (!getReply.value().node->nodes || getReply.value().node->nodes->empty()) {
                    throw std::runtime_error("Incorrect get node nodes");
                }

                if((*getReply.value().node->nodes)[0].value != "root") {
                    throw std::runtime_error("Incorrect root order");
                }

                if((*getReply.value().node->nodes)[1].value != "one_value") {
                    throw std::runtime_error("Incorrect one_value order");
                }

                if((*getReply.value().node->nodes)[2].value != "two_value") {
                    throw std::runtime_error("Incorrect two_value order");
                }

                if((*getReply.value().node->nodes)[3].value != "a_value") {
                    throw std::runtime_error("Incorrect a_value order");
                }

                auto delReply = co_await _etcd->del("test_key_dir", true, true);
                if (!delReply) {
                    throw std::runtime_error("");
                }

                if (delReply.value().errorCode) {
                    throw std::runtime_error("Incorrect del errorCode");
                }
            }
            // TTL test
            {
                auto putReply = co_await _etcd->put("ttl_key", "value", {}, {}, {}, 1, false, false, false);
                if (!putReply) {
                    throw std::runtime_error("");
                }

                if (!putReply.value().node || putReply.value().node->value != "value") {
                    throw std::runtime_error("Incorrect put dir node value");
                }

                if (putReply.value().node->ttl != 1) {
                    throw std::runtime_error("Incorrect put dir node ttl");
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1500));

                auto getReply = co_await _etcd->get("ttl_key", false, false, false, {});
                if(!getReply) {
                    throw std::runtime_error("");
                }

                if (!getReply.value().errorCode || getReply.value().errorCode != EtcdErrorCodes::KEY_DOES_NOT_EXIST) {
                    throw std::runtime_error("Incorrect ttl node errorCode");
                }
            }
            // Watch test
            {
                {
                    auto putReply = co_await _etcd->put("watch_key", "value", {}, {}, {}, {}, false, false, false);
                    if (!putReply) {
                        throw std::runtime_error("");
                    }

                    if (!putReply.value().node || putReply.value().node->value != "value") {
                        throw std::runtime_error("Incorrect put dir node value");
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
                        throw std::runtime_error("");
                    }

                    if (!putReply.value().node || putReply.value().node->value != "new_value") {
                        throw std::runtime_error("Incorrect put dir node value");
                    }

                    co_return IchorBehaviour::DONE;
                });

                auto getReply = co_await _etcd->get("watch_key", false, false, true, {});
                if(!getReply) {
                    throw std::runtime_error("");
                }

                if (!getReply.value().node || getReply.value().node->value != "new_value") {
                    throw std::runtime_error("Incorrect get node value");
                }
            }
            // Health test
            {
                auto healthReply = co_await _etcd->health();
                if (!healthReply) {
                    throw std::runtime_error("");
                }

                if (!healthReply.value()) {
                    throw std::runtime_error("Incorrect health value");
                }
            }
            // Version test
            {
                auto versionReply = co_await _etcd->version();
                if (!versionReply) {
                    throw std::runtime_error("");
                }

                if (versionReply.value().etcdcluster.empty()) {
                    throw std::runtime_error("Incorrect etcdcluster value");
                }

                if (versionReply.value().etcdserver.empty()) {
                    throw std::runtime_error("Incorrect etcdcluster value");
                }
            }

            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());

            co_return {};
        }

        void addDependencyInstance(IEtcd &Etcd, IService&) {
            _etcd = &Etcd;
        }

        void removeDependencyInstance(IEtcd&, IService&) {
            _etcd = nullptr;
        }

        IEtcd *_etcd;
    };
}
