#include <exception>
#include "GlobalRealtimeSettings.h"
#include <fmt/core.h>
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
//TODO
#else
#include <sys/mman.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>
#include <array>
#include <cstring>
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
        std::array<char, 4> control_read{};
        smt_file = open("/sys/devices/system/cpu/smt/control", O_RDWR);
        if(smt_file == -1) {
            fmt::print("Could not disable SMT\n");
            reenable_smt = false;
            close(smt_file);
            goto end_smt;
        }

        auto size = read(smt_file, control_read.data(), 4);
        if(size == -1) {
            fmt::print("smt_file read\n");
            std::terminate();
        }

        if(lseek(smt_file, 0, SEEK_SET) == -1) {
            fmt::print("smt_file lseek\n");
            std::terminate();
        }

        if(strncmp(control_read.data(), "off", 3) == 0 || strncmp(control_read.data(), "notimplemented", 14) == 0) {
            reenable_smt = false;
            close(smt_file);
        } else {
            auto ret = write(smt_file, target_control, 3);
            if (ret == -1) {
                fmt::print("smt_file write\n");
                std::terminate();
            }
        }
    } else {
        fmt::print("SMT missing\n");
    }

    end_smt:
    if(setpriority(PRIO_PROCESS, 0, -20) != 0) {
        fmt::print("setpriority failed\n");
    }
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

    if(::stat("/dev/cpu_dma_latency", &statbuf) == 0) {
        close(dma_latency_file);
    }
#endif
}
