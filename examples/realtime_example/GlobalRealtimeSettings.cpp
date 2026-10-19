#include "GlobalRealtimeSettings.h"
#include <fmt/core.h>
#include <cerrno>
#include <exception>
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
//TODO
#else
#include <sys/mman.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>
#include <array>
#include <cstring>
#include <string_view>
#endif

GlobalRealtimeSettings::GlobalRealtimeSettings() {
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
    //TODO
#else

    // if possible, tell linux to not go into power saving states
    if(::stat("/dev/cpu_dma_latency", &statbuf) == 0) {
        int target_latency = 0;
        dma_latency_file = open("/dev/cpu_dma_latency", O_RDWR);
        if(dma_latency_file == -1) {
            fmt::print("dma_latency_file\n");
            std::terminate();
        }
        auto ret = write(dma_latency_file, &target_latency, 4);
        if(ret == -1) {
            fmt::print("dma_latency_file write\n");
            std::terminate();
        }
    }

    // tell linux not to swap out the memory used by this process
    int rc = mlockall(MCL_CURRENT | MCL_FUTURE);
    if (rc != 0) {
        fmt::print("mlockall {} {}\n", rc, errno);
        std::terminate();
    }

    // disable smt if possible
    if(::stat("/sys/devices/system/cpu/smt/control", &statbuf) == 0) {
        const char *target_control = "off";
        std::array<char, 32> control_read{};
        smt_file = open("/sys/devices/system/cpu/smt/control", O_RDWR);
        if(smt_file == -1) {
            fmt::print("Could not disable SMT\n");
            reenable_smt = false;
        } else {
            auto size = read(smt_file, control_read.data(), control_read.size() - 1);
            if(size == -1) {
                fmt::print("smt_file read\n");
                std::terminate();
            }
            std::string_view control{control_read.data(), static_cast<size_t>(size)};
            if(control.ends_with('\n')) {
                control.remove_suffix(1);
            }

            if(lseek(smt_file, 0, SEEK_SET) == -1) {
                fmt::print("smt_file lseek\n");
                std::terminate();
            }

            // Only "on" can be turned off, the others are off already or can't be changed: off, forceoff, notsupported, notimplemented
            if(control != "on") {
                reenable_smt = false;
                close(smt_file);
                smt_file = -1;
            } else {
                auto ret = write(smt_file, target_control, 3);
                if (ret == -1) {
                    fmt::print("smt_file write\n");
                    std::terminate();
                }
            }
        }
    } else {
        reenable_smt = false;
        fmt::print("SMT missing\n");
    }

    if(setpriority(PRIO_PROCESS, 0, -20) != 0) {
        fmt::print("setpriority failed\n");
    }
    fmt::print("finished settings\n");
#endif
}

GlobalRealtimeSettings::~GlobalRealtimeSettings() {
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
    //TODO
#else
    int rc = munlockall();
    if (rc != 0) {
        fmt::print("munlockall\n");
        std::terminate();
    }

    if(reenable_smt) {
        const char *target_control = "on";
        auto ret = write(smt_file, target_control, 3);
        if (ret == -1) {
            fmt::print("reenable_smt write\n");
            std::terminate();
        }
        close(smt_file);
    }

    if(dma_latency_file >= 0) {
        close(dma_latency_file);
    }
#endif
}
