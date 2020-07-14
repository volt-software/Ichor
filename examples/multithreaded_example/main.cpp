#include "OneService.h"
#include "OtherService.h"
#include <optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include <optional_bundles/logging_bundle/LoggerAdmin.h>
#include <optional_bundles/logging_bundle/SpdlogLogger.h>
#include <framework/CommunicationChannel.h>
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    auto start = std::chrono::system_clock::now();

    CommunicationChannel channel{};

    std::thread t1([&channel](){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        DependencyManager dm{};
        channel.addManager(&dm);
        auto logMgr = dm.createServiceManager<IFrameworkLogger, SpdlogFrameworkLogger>();
        auto logAdminMgr = dm.createServiceManager<ILoggerAdmin, LoggerAdmin<SpdlogLogger>>();
        auto oneService = dm.createServiceManager<IOneService, OneService>(RequiredList<ILogger>, OptionalList<>);
        dm.start();
    });

    std::thread t2([&channel](){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        DependencyManager dm{};
        channel.addManager(&dm);
        auto logMgr = dm.createServiceManager<IFrameworkLogger, SpdlogFrameworkLogger>();
        auto logAdminMgr = dm.createServiceManager<ILoggerAdmin, LoggerAdmin<SpdlogLogger>>();
        auto otherService = dm.createServiceManager<IOtherService, OtherService>(RequiredList<ILogger>, OptionalList<>);
        dm.start();
    });

    t1.join();
    t2.join();

    auto end = std::chrono::system_clock::now();
    std::cout << fmt::format("Program ran for {:n} µs\n", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}