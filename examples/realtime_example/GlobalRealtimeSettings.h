#pragma once

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
//TODO
#else
#include <sys/stat.h>
#endif

class GlobalRealtimeSettings {
public:
    GlobalRealtimeSettings();
    ~GlobalRealtimeSettings();

private:
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
    //TODO
#else
    struct stat statbuf{};
    int smt_file;
    int dma_latency_file;
#endif
    bool reenable_smt{true};
};