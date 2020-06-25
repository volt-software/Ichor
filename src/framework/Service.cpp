#include <spdlog/spdlog.h>
#include "framework/Service.h"

std::atomic<uint64_t> Cppelix::Service::_serviceIdCounter = 0;

Cppelix::Service::Service() noexcept : IService(), _serviceId(_serviceIdCounter++), _serviceGid(sole::uuid4()), _serviceState(ServiceState::INSTALLED) {

}

Cppelix::Service::~Service() {
    _serviceId = 0;
    _serviceGid.ab = 0;
    _serviceGid.cd = 0;
    _serviceState = ServiceState::UNINSTALLED;
}


bool Cppelix::Service::internal_start() {
    if(_serviceState != ServiceState::INSTALLED) {
        return false;
    }

    _serviceState = ServiceState::STARTING;
    if(start()) {
        _serviceState = ServiceState::ACTIVE;
        return true;
    } else {
        _serviceState = ServiceState::INSTALLED;
    }

    return false;
}

bool Cppelix::Service::internal_stop() {
    if(_serviceState != ServiceState::ACTIVE) {
        return true;
    }

    _serviceState = ServiceState::STOPPING;
    if(stop()) {
        _serviceState = ServiceState::INSTALLED;
        return true;
    } else {
        _serviceState = ServiceState::UNKNOWN;
    }

    return false;
}

Cppelix::ServiceState Cppelix::Service::getState() const noexcept {
    return _serviceState;
}
