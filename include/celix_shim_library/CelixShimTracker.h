#pragma once

namespace Cppelix {
    struct CelixShimTracker {
        const char* serviceName;
        void* callbackHandle;
        void (*add)(void* handle, void* svc);
        void (*remove)(void* handle, void* svc);
    };
}