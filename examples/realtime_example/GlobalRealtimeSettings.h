#pragma once

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
//TODO
#else
#include <sys/stat.h>
#endif

class GlobalRealtimeSettings {
public:
    GlobalRealtimeSettings();
    GlobalRealtimeSettings(const GlobalRealtimeSettings&) = delete;
    GlobalRealtimeSettings(GlobalRealtimeSettings&&) = delete;
    GlobalRealtimeSettings& operator=(const GlobalRealtimeSettings&) = delete;
    GlobalRealtimeSettings& operator=(GlobalRealtimeSettings&&) = delete;
    ~GlobalRealtimeSettings();

private:
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
    //TODO
#else
    struct stat statbuf{};
    int smt_file{-1};
    int dma_latency_file{-1};
#endif
    bool reenable_smt{true};
};