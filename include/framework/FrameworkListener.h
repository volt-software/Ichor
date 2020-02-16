#pragma once

#include <unordered_map>
#include <vector>

namespace Cppelix {
    enum class FrameworkEvent {
        ERROR,
        INFO,
        PACKAGES_REFRESHED,
        STARTED,
        STARTLEVEL_CHANGED,
        STOPPED,
        STOPPED_BOOTCLASSPATH_MODIFIED,
        STOPPED_SYSTEM_REFRESHED,
        STOPPED_UPDATE,
        WAIT_TIMEDOUT,
        WARNING
    };

    class FrameworkListener {
    public:

    private:
    };
}