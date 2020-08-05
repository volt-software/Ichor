#include <optional_bundles/network_bundle/tcp/TcpClientAdmin.h>
#include <optional_bundles/network_bundle/IConnectionService.h>

bool Cppelix::TcpClientAdmin::start() {
    _trackerRegistration = getManager()->registerDependencyTracker<IConnectionService>(getServiceId(), this);

    return false;
}

bool Cppelix::TcpClientAdmin::stop() {
    return false;
}

void Cppelix::TcpClientAdmin::addDependencyInstance(Cppelix::ILogger *logger) {

}

void Cppelix::TcpClientAdmin::removeDependencyInstance(Cppelix::ILogger *logger) {

}

void Cppelix::TcpClientAdmin::handleDependencyRequest(Cppelix::IConnectionService *,
                                                      const Cppelix::DependencyRequestEvent *const evt) {
    if(!evt->properties->contains("Address")) {
        throw std::runtime_error("Missing address");
    }

    if(!evt->properties->contains("Port")) {
        throw std::runtime_error("Missing port");
    }

    auto newProps = *evt->properties;
    newProps.emplace("Filter", Filter{ServiceIdFilterEntry{evt->originatingService}});

    if(!_connections.contains(evt->originatingService)) {
        _connections.emplace(evt->originatingService, getManager()->createServiceManager<IConnectionService, TcpConnectionService>(RequiredList<ILogger>, OptionalList<>, newProps));
    }
}

void Cppelix::TcpClientAdmin::handleDependencyUndoRequest(Cppelix::IConnectionService *,
                                                          const Cppelix::DependencyUndoRequestEvent *const evt) {
    auto connection = _connections.find(evt->originatingService);

    if(connection != end(_connections)) {
        getManager()->pushEvent<RemoveServiceEvent>(evt->originatingService, connection->second->getServiceId());
        _connections.erase(connection);
    }
}

