#include "TestService.h"
#include "TestMsgJsonSerializer.h"
#include <optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include <chrono>
#include <iostream>

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));

    auto start = std::chrono::system_clock::now();
    Framework fw{{}};
    DependencyManager dm{};
    auto logMgr = dm.createServiceManager<IFrameworkLogger, SpdlogFrameworkLogger>();
    auto serAdmin = dm.createDependencyServiceManager<ISerializationAdmin, SerializationAdmin>(RequiredList<IFrameworkLogger>, OptionalList<>);
    auto testMsgSerializer = dm.createDependencyServiceManager<ISerializer, TestMsgJsonSerializer>(RequiredList<IFrameworkLogger, ISerializationAdmin>, OptionalList<>);
    auto testMgr = dm.createDependencyServiceManager<ITestService, TestService>(RequiredList<IFrameworkLogger, ISerializationAdmin>, OptionalList<>);
    dm.start();
    auto end = std::chrono::system_clock::now();
    std::cout << fmt::format("Program ran for {:n} µs\n", std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

    return 0;
}