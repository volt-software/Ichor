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
        SPDLOG_ERROR("Cannot start bundle {}, current state {}", _bundleId, _bundleState);
        return false;
    }

    _bundleState = STARTING;
    if(start()) {
        _bundleState = ACTIVE;
        SPDLOG_INFO("Bundle {} started", _bundleId);
        return true;
    } else {
        _bundleState = INSTALLED;
        SPDLOG_ERROR("Bundle {} failed to start", _bundleId);
    }

    return false;
}

bool Cppelix::Bundle::internal_stop() {
    if(_bundleState != ACTIVE) {
        SPDLOG_ERROR("Cannot stop bundle {}, current state {}", _bundleId, _bundleState);
        return false;
    }

    _bundleState = STOPPING;
    if(start()) {
        _bundleState = INSTALLED;
        SPDLOG_INFO("Bundle {} stopped", _bundleId);
        return true;
    } else {
        _bundleState = UNKNOWN;
        SPDLOG_ERROR("Bundle {} failed to start", _bundleId);
    }

    return false;
}
