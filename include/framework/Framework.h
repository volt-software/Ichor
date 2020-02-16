#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include "ServiceListener.h"
#include "BundleListener.h"
#include "FrameworkListener.h"
#include "Bundle.h"

namespace Cppelix {
    class Framework {
    public:
        explicit Framework(std::unordered_map<std::string, std::string> configurationMap);

    private:
        std::unordered_map<std::string, std::string> _configurationMap;
        std::vector<ServiceListener> _serviceListeners;
        std::vector<BundleListener> _bundleListeners;
        std::vector<FrameworkListener> _frameworkListeners;
        std::unique_ptr<Bundle> _bundle;
        std::mutex _globalMutex;

        static bool _globallyInitialized;
    };
}