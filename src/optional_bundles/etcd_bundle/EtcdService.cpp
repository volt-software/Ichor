#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/etcd_bundle/EtcdService.h>
#include <ichor/optional_bundles/etcd_bundle/rpc.pb.h>
#include <ichor/optional_bundles/etcd_bundle/kv.pb.h>
#include <ichor/optional_bundles/etcd_bundle/auth.pb.h>
#include <grpc++/grpc++.h>


Ichor::EtcdService::EtcdService(DependencyRegister &reg, IchorProperties props, DependencyManager *mng) : Service(std::move(props), mng) {
    reg.registerDependency<ILogger>(this, true);
}

bool Ichor::EtcdService::start() {
    auto addressProp = getProperties()->find("EtcdAddress");
    if(addressProp == end(*getProperties())) {
        throw std::runtime_error("Missing EtcdAddress");
    }

    _channel = grpc::CreateChannel(Ichor::any_cast<std::string>(addressProp->second), grpc::InsecureChannelCredentials());
    _stub = etcdserverpb::KV::NewStub(_channel);
    return true;
}

bool Ichor::EtcdService::stop() {
    _stub = nullptr;
    _channel = nullptr;
    return true;
}

void Ichor::EtcdService::addDependencyInstance(ILogger *logger) {
    _logger = logger;
    ICHOR_LOG_TRACE(_logger, "Inserted logger");
}

void Ichor::EtcdService::removeDependencyInstance(ILogger *logger) {
    _logger = nullptr;
}

bool Ichor::EtcdService::put(std::string&& key, std::string&& value) {
    grpc::ClientContext context{};
    etcdserverpb::PutRequest req{};
    req.set_key(std::move(key));
    req.set_value(std::move(value));
    etcdserverpb::PutResponse resp{};
    return _stub->Put(&context, req, &resp).ok();
}

std::optional<std::string> Ichor::EtcdService::get(std::string &&key) {
    grpc::ClientContext context{};
    etcdserverpb::RangeRequest req{};
    etcdserverpb::RangeResponse resp{};
    req.set_key(std::move(key));

    if(_stub->Range(&context, req, &resp).ok()) {
        return resp.kvs().Get(0).value();
    }

    return {};
}
