#pragma once

#include <unordered_map>
#include <vector>

namespace Cppelix {
    enum class BundleEvent {
        INSTALLED,
        LAZY_ACTIVATION,
        RESOLVED,
        STARTED,
        STARTING,
        STOPPED,
        STOPPING,
        UNINSTALLED,
        UNRESOLVED,
        UPDATED
    };

    class BundleListener {
    public:

    private:
    };
}