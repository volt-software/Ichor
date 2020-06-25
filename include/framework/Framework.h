#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include "ServiceListener.h"
#include "ServiceListener.h"
#include "FrameworkListener.h"
#include "Service.h"

namespace Cppelix {
    class Framework {
    public:
        explicit Framework(std::unordered_map<std::string, std::string> configurationMap);

    private:
        std::unordered_map<std::string, std::string> _configurationMap;
        std::vector<ServiceListener> _serviceListeners;
        std::vector<ServiceListener> _bundleListeners;
        std::vector<FrameworkListener> _frameworkListeners;
        std::unique_ptr<Service> _bundle;
        std::mutex _globalMutex;

        static bool _globallyInitialized;
    };
}