#include "framework/Framework.h"

bool Cppelix::Framework::_globallyInitialized = false;

Cppelix::Framework::Framework(std::unordered_map<std::string, std::string> configurationMap) : _configurationMap(move(configurationMap)) {
    std::scoped_lock lock(_globalMutex);
    if(!_globallyInitialized) {


        _globallyInitialized = true;
    }

    //_service = std::make_unique<Service>();
}
