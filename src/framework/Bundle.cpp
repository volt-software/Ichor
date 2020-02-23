#include <spdlog/spdlog.h>
#include "framework/Bundle.h"

std::atomic<uint64_t> Cppelix::Bundle::_bundleIdCounter = 0;

Cppelix::Bundle::Bundle() noexcept : _bundleId(_bundleIdCounter++), _bundleGid(sole::uuid4()), _bundleState(BundleState::INSTALLED) {

}

Cppelix::Bundle::~Bundle() {
    _bundleId = 0;
    _bundleGid.ab = 0;
    _bundleGid.cd = 0;
    _bundleState = BundleState::UNINSTALLED;
}


bool Cppelix::Bundle::internal_start() {
    if(_bundleState != BundleState::INSTALLED) {
        return false;
    }

    _bundleState = BundleState::STARTING;
    if(start()) {
        _bundleState = BundleState::ACTIVE;
        return true;
    } else {
        _bundleState = BundleState::INSTALLED;
    }

    return false;
}

bool Cppelix::Bundle::internal_stop() {
    if(_bundleState != BundleState::ACTIVE) {
        return false;
    }

    _bundleState = BundleState::STOPPING;
    if(start()) {
        _bundleState = BundleState::INSTALLED;
        return true;
    } else {
        _bundleState = BundleState::UNKNOWN;
    }

    return false;
}

Cppelix::BundleState Cppelix::Bundle::getState() const noexcept {
    return _bundleState;
}
