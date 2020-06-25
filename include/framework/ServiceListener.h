#pragma once

#include <unordered_map>
#include <vector>

namespace Cppelix {
    enum class ServiceEvent {
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

    class ServiceListener {
    public:

    private:
    };
}