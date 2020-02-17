#include <spdlog/spdlog.h>
#include "framework/Bundle.h"

std::atomic<uint64_t> Cppelix::Bundle::_bundleIdCounter = 0;

Cppelix::Bundle::Bundle() : _bundleId(_bundleIdCounter++), _bundleGid(sole::uuid4()), _bundleState(INSTALLED) {

}

Cppelix::Bundle::~Bundle() {
    _bundleId = 0;
    _bundleGid.ab = 0;
    _bundleGid.cd = 0;
    _bundleState = UNINSTALLED;
}


bool Cppelix::Bundle::internal_start() {
    if(_bundleState != INSTALLED) {
        return false;
    }

    _bundleState = STARTING;
    if(start()) {
        _bundleState = ACTIVE;
        return true;
    } else {
        _bundleState = INSTALLED;
    }

    return false;
}

bool Cppelix::Bundle::internal_stop() {
    if(_bundleState != ACTIVE) {
        return false;
    }

    _bundleState = STOPPING;
    if(start()) {
        _bundleState = INSTALLED;
        return true;
    } else {
        _bundleState = UNKNOWN;
    }

    return false;
}

Cppelix::BundleState Cppelix::Bundle::getState() {
    return _bundleState;
}
