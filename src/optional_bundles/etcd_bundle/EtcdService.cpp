#include <framework/DependencyManager.h>
#include <optional_bundles/etcd_bundle/EtcdService.h>
#include <optional_bundles/etcd_bundle/rpc.pb.h>
#include <optional_bundles/etcd_bundle/kv.pb.h>
#include <optional_bundles/etcd_bundle/auth.pb.h>
#include <grpc++/grpc++.h>

bool Cppelix::EtcdService::start() {
    auto addressProp = getProperties()->find("EtcdAddress");
    if(addressProp == end(*getProperties())) {
        throw std::runtime_error("Missing EtcdAddress");
    }

    _channel = grpc::CreateChannel(std::any_cast<std::string>(addressProp->second), grpc::InsecureChannelCredentials());
    _stub = etcdserverpb::KV::NewStub(_channel);
    return true;
}

bool Cppelix::EtcdService::stop() {
    _stub = nullptr;
    _channel = nullptr;
    return true;
}

void Cppelix::EtcdService::addDependencyInstance(Cppelix::ILogger *logger) {
    _logger = logger;
    LOG_TRACE(_logger, "Inserted logger");
}

void Cppelix::EtcdService::removeDependencyInstance(Cppelix::ILogger *logger) {
    _logger = nullptr;
}

bool Cppelix::EtcdService::put(std::string&& key, std::string&& value) {
    grpc::ClientContext context{};
    etcdserverpb::PutRequest req{};
    req.set_key(std::move(key));
    req.set_value(std::move(value));
    etcdserverpb::PutResponse resp{};
    return _stub->Put(&context, req, &resp).ok();
}

std::optional<std::string> Cppelix::EtcdService::get(std::string &&key) {
    grpc::ClientContext context{};
    etcdserverpb::RangeRequest req{};
    etcdserverpb::RangeResponse resp{};
    req.set_key(std::move(key));

    if(_stub->Range(&context, req, &resp).ok()) {
        return resp.kvs().Get(0).value();
    }

    return {};
}
