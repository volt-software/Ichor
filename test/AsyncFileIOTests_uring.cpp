#include "Common.h"
#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/logging/CoutLogger.h>
#include <fstream>
#include <filesystem>
#include <ichor/event_queues/IOUringQueue.h>
#include <ichor/services/io/IOUringAsyncFileIO.h>
#include <ichor/stl/LinuxUtils.h>
#include <catch2/generators/catch_generators.hpp>
#include "../examples/common/DebugService.h"

#define IOIMPL IOUringAsyncFileIO
#define QIMPL IOUringQueue

struct AsyncFileIOExpensiveSetup {
    AsyncFileIOExpensiveSetup() {
        std::ofstream out("BigFile.txt", std::ios::app);
        while(out.tellp() < 8192ll*1024ll) {
            REQUIRE(!out.fail());
            out << "This is a test123";
        }
        REQUIRE(!out.fail());
        bigFileFilesize = out.tellp();
        REQUIRE(bigFileFilesize >= 8192*1024);
        out.close();
    }

    int64_t bigFileFilesize{};
};

namespace {
    tl::optional<Version> emulateKernelVersion;
}

TEST_CASE_METHOD(AsyncFileIOExpensiveSetup, "AsyncFileIOTests_uring: Reading non-existent file should error") {
    auto version = Ichor::v1::kernelVersion();

    REQUIRE(version);
    if(version < v1::Version{5, 18, 0}) {
        SKIP("Kernel version too old to run test.");
        return;
    }

    auto gen_i = GENERATE(1, 2);

    if(gen_i == 2) {
        emulateKernelVersion = v1::Version{5, 18, 0};
        fmt::println("emulating kernel version {}", *emulateKernelVersion);
    } else {
        fmt::println("kernel version {}", *version);
    }

    fmt::print("section 1\n");
    ServiceIdType dbgSvcId{};
    auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
    auto &dm = queue->createManager();
    ServiceIdType ioSvcId{};

    std::thread t([&]() {
        REQUIRE(queue->createEventLoop());
        dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
        dbgSvcId = dm.createServiceManager<DebugService, IDebugService>()->getServiceId();
        ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
        queue->start(CaptureSigInt);
    });

    waitForRunning(dm);

    runForOrQueueEmpty(dm);

    queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, [&]() -> AsyncGenerator<IchorBehaviour> {
        fmt::print("run function co_await\n");
        auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
        REQUIRE(async_io_svc);
        tl::expected<std::string, Ichor::v1::IOError> ret = co_await async_io_svc->first->readWholeFile("NonExistentFile.txt");
        if(!ret) {
            fmt::println("readWholeFile error {}", ret.error());
        } else {
            fmt::println("readWholeFile found an existing file.");
        }
        REQUIRE(!ret);
        REQUIRE(ret.error() == IOError::FILE_DOES_NOT_EXIST);
        queue->pushEvent<QuitEvent>(ServiceIdType{0});
        co_return {};
    });

    auto start = std::chrono::steady_clock::now();
    while(queue->is_running()) {
        std::this_thread::sleep_for(10ms);
        auto now = std::chrono::steady_clock::now();
        if(now - start >= 20s) {
            queue->pushEvent<RunFunctionEvent>(ServiceIdType{0}, [&]() {
                auto dbgSvc = dm.getService<IDebugService>(dbgSvcId);
                REQUIRE(dbgSvc);
                dbgSvc->first->printServices();
            });
            std::this_thread::sleep_for(10ms);
            break;
        }
    }

    auto now = std::chrono::steady_clock::now();
    REQUIRE(now - start < 20s);

    t.join();
}

TEST_CASE_METHOD(AsyncFileIOExpensiveSetup, "AsyncFileIOTests_uring: Reading file without permissions should error") {
    auto version = Ichor::v1::kernelVersion();

    REQUIRE(version);
    if(version < v1::Version{5, 18, 0}) {
        SKIP("Kernel version too old to run test.");
        return;
    }

    auto gen_i = GENERATE(1, 2);

    if(gen_i == 2) {
        emulateKernelVersion = v1::Version{5, 18, 0};
        fmt::println("emulating kernel version {}", *emulateKernelVersion);
    } else {
        fmt::println("kernel version {}", *version);
    }

    fmt::print("section 1\n");
    auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
    auto &dm = queue->createManager();
    ServiceIdType ioSvcId{};

    std::thread t([&]() {
        // moved to inside thread, because REQUIRE() isn't thread-safe -.-
        {
            std::remove("NoPermIO.txt"); // ignore error, each OS handles this differently and most of the time it's just because it didn't exist in the first place

            std::ofstream out("NoPermIO.txt");
            out << "This is a test";
            out.close();

            std::error_code ec;
            std::filesystem::permissions("NoPermIO.txt", std::filesystem::perms::none, ec);
            REQUIRE(!ec); // if running tests as root, this will fail
        }

        REQUIRE(queue->createEventLoop());
        dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
        ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
        queue->start(CaptureSigInt);
    });

    waitForRunning(dm);

    runForOrQueueEmpty(dm);

    queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, [&]() -> AsyncGenerator<IchorBehaviour> {
        fmt::print("run function co_await\n");
        auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
        tl::expected<std::string, Ichor::v1::IOError> ret = co_await async_io_svc->first->readWholeFile("NoPermIO.txt");
        if(!ret) {
            fmt::println("readWholeFile error {}", ret.error());
        } else {
            fmt::println("readWholeFile found an existing file. {}", (*ret).size());
        }
        REQUIRE(!ret);
        REQUIRE(ret.error() == IOError::NO_PERMISSION);
        queue->pushEvent<QuitEvent>(ServiceIdType{0});
        co_return {};
    });

    auto start = std::chrono::steady_clock::now();
    while(queue->is_running()) {
        std::this_thread::sleep_for(10ms);
        auto now = std::chrono::steady_clock::now();
        REQUIRE(now - start < 20s);
    }

    t.join();

    std::remove("NoPermIO.txt"); // ignore error, each OS handles this differently and most of the time it's just because it didn't exist in the first place
}

TEST_CASE_METHOD(AsyncFileIOExpensiveSetup, "AsyncFileIOTests_uring: Read whole file Small") {
    auto version = Ichor::v1::kernelVersion();

    REQUIRE(version);
    if(version < v1::Version{5, 18, 0}) {
        SKIP("Kernel version too old to run test.");
        return;
    }

    auto gen_i = GENERATE(1, 2);

    if(gen_i == 2) {
        emulateKernelVersion = v1::Version{5, 18, 0};
        fmt::println("emulating kernel version {}", *emulateKernelVersion);
    } else {
        fmt::println("kernel version {}", *version);
    }

    fmt::print("section 2a\n");
    auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
    auto &dm = queue->createManager();
    ServiceIdType ioSvcId{};

    std::thread t([&]() {
        REQUIRE(queue->createEventLoop());
        dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
        ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
        queue->start(CaptureSigInt);
    });

    {
        std::ofstream out("AsyncFileIO.txt");
        out << "This is a test";
        out.close();
    }

    waitForRunning(dm);

    runForOrQueueEmpty(dm);

    queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, [&]() -> AsyncGenerator<IchorBehaviour> {
        fmt::print("run function co_await\n");
        auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
        tl::expected<std::string, Ichor::v1::IOError> ret = co_await async_io_svc->first->readWholeFile("AsyncFileIO.txt");
        fmt::print("require\n");
        REQUIRE(ret == "This is a test");
        queue->pushEvent<QuitEvent>(ServiceIdType{0});
        co_return {};
    });

    auto start = std::chrono::steady_clock::now();
    while(queue->is_running()) {
        std::this_thread::sleep_for(10ms);
        auto now = std::chrono::steady_clock::now();
        REQUIRE(now - start < 20s);
    }

    t.join();
}

TEST_CASE_METHOD(AsyncFileIOExpensiveSetup, "AsyncFileIOTests_uring: Read whole file large") {
    auto version = Ichor::v1::kernelVersion();

    REQUIRE(version);
    if(version < v1::Version{5, 18, 0}) {
        SKIP("Kernel version too old to run test.");
        return;
    }

    auto gen_i = GENERATE(1, 2);

    if(gen_i == 2) {
        emulateKernelVersion = v1::Version{5, 18, 0};
        fmt::println("emulating kernel version {}", *emulateKernelVersion);
    } else {
        fmt::println("kernel version {}", *version);
    }

    fmt::println("section 2b");
    auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
    auto &dm = queue->createManager();
    ServiceIdType ioSvcId{};

    std::thread t([&]() {
        REQUIRE(queue->createEventLoop());
        dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
        ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
        queue->start(CaptureSigInt);
    });

    waitForRunning(dm);

    runForOrQueueEmpty(dm);

    queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, [&]() -> AsyncGenerator<IchorBehaviour> {
        fmt::println("run function co_await");
        auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
        tl::expected<std::string, Ichor::v1::IOError> ret = co_await async_io_svc->first->readWholeFile("BigFile.txt");
        if(!ret) {
            fmt::println("readWholeFile error {}\n", ret.error());
        } else {
            fmt::println("readWholeFile found an existing file.");
        }
        REQUIRE(ret);
        REQUIRE((*ret).size() == (uint64_t)bigFileFilesize);
        queue->pushEvent<QuitEvent>(ServiceIdType{0});
        co_return {};
    });

    auto start = std::chrono::steady_clock::now();
    while(queue->is_running()) {
        std::this_thread::sleep_for(10ms);
        auto now = std::chrono::steady_clock::now();
        REQUIRE(now - start < 20s);
    }

    t.join();
}

TEST_CASE_METHOD(AsyncFileIOExpensiveSetup, "AsyncFileIOTests_uring: Copying non-existent file should error") {
    auto version = Ichor::v1::kernelVersion();

    REQUIRE(version);
    if(version < v1::Version{5, 18, 0}) {
        SKIP("Kernel version too old to run test.");
        return;
    }

    auto gen_i = GENERATE(1, 2);

    if(gen_i == 2) {
        emulateKernelVersion = v1::Version{5, 18, 0};
        fmt::println("emulating kernel version {}", *emulateKernelVersion);
    } else {
        fmt::println("kernel version {}", *version);
    }

    fmt::print("section 3\n");
    auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
    auto &dm = queue->createManager();
    ServiceIdType ioSvcId{};

    std::thread t([&]() {
        REQUIRE(queue->createEventLoop());
        dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
        ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
        queue->start(CaptureSigInt);
    });

    waitForRunning(dm);

    runForOrQueueEmpty(dm);

    queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, [&]() -> AsyncGenerator<IchorBehaviour> {
        fmt::print("run function co_await\n");
        auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
        tl::expected<void, Ichor::v1::IOError> ret = co_await async_io_svc->first->copyFile("NonExistentFile.txt", "DestinationNull.txt");
        fmt::print("require\n");
        REQUIRE(!ret);
        REQUIRE(ret.error() == IOError::FILE_DOES_NOT_EXIST);
        queue->pushEvent<QuitEvent>(ServiceIdType{0});
        co_return {};
    });

    auto start = std::chrono::steady_clock::now();
    while(queue->is_running()) {
        std::this_thread::sleep_for(10ms);
        auto now = std::chrono::steady_clock::now();
        REQUIRE(now - start < 20s);
    }

    t.join();
}

TEST_CASE_METHOD(AsyncFileIOExpensiveSetup, "AsyncFileIOTests_uring: Copying file") {
    auto version = Ichor::v1::kernelVersion();

    REQUIRE(version);
    if(version < v1::Version{5, 18, 0}) {
        SKIP("Kernel version too old to run test.");
        return;
    }

    auto gen_i = GENERATE(1, 2);

    if(gen_i == 2) {
        emulateKernelVersion = v1::Version{5, 18, 0};
        fmt::println("emulating kernel version {}", *emulateKernelVersion);
    } else {
        fmt::println("kernel version {}", *version);
    }

    fmt::print("section 4\n");
    auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
    auto &dm = queue->createManager();
    ServiceIdType ioSvcId{};

    std::thread t([&]() {
        REQUIRE(queue->createEventLoop());
        dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
        ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
        queue->start(CaptureSigInt);
    });

    {
        std::ofstream out("AsyncFileIO.txt");
        out << "This is a test";
        out.close();

        std::remove("Destination.txt"); // ignore error, each OS handles this differently and most of the time it's just because it didn't exist in the first place
    }

    waitForRunning(dm);

    runForOrQueueEmpty(dm);

    queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, [&]() -> AsyncGenerator<IchorBehaviour> {
        fmt::print("run function co_await\n");
        auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
        {
            tl::expected<void, Ichor::v1::IOError> ret = co_await async_io_svc->first->copyFile("AsyncFileIO.txt", "Destination.txt");
            fmt::print("require\n");
            REQUIRE(ret);
        }
        tl::expected<std::string, Ichor::v1::IOError> ret = co_await async_io_svc->first->readWholeFile("Destination.txt");
        REQUIRE(ret == "This is a test");
        queue->pushEvent<QuitEvent>(ServiceIdType{0});
        co_return {};
    });

    auto start = std::chrono::steady_clock::now();
    while(queue->is_running()) {
        std::this_thread::sleep_for(10ms);
        auto now = std::chrono::steady_clock::now();
        REQUIRE(now - start < 20s);
    }

    t.join();
}

TEST_CASE_METHOD(AsyncFileIOExpensiveSetup, "AsyncFileIOTests_uring: Copying large file") {
    auto version = Ichor::v1::kernelVersion();

    REQUIRE(version);
    if(version < v1::Version{5, 18, 0}) {
        SKIP("Kernel version too old to run test.");
        return;
    }

    auto gen_i = GENERATE(1, 2);

    if(gen_i == 2) {
        emulateKernelVersion = v1::Version{5, 18, 0};
        fmt::println("emulating kernel version {}", *emulateKernelVersion);
    } else {
        fmt::println("kernel version {}", *version);
    }

    fmt::print("section 5\n");
    auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
    auto &dm = queue->createManager();
    ServiceIdType ioSvcId{};

    std::thread t([&]() {
        REQUIRE(queue->createEventLoop());
        dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
        ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
        queue->start(CaptureSigInt);
    });

    std::remove("Destination.txt"); // ignore error, each OS handles this differently and most of the time it's just because it didn't exist in the first place

    waitForRunning(dm);

    runForOrQueueEmpty(dm);

    queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, [&]() -> AsyncGenerator<IchorBehaviour> {
        fmt::print("run function co_await\n");
        auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
        {
            tl::expected<void, Ichor::v1::IOError> ret = co_await async_io_svc->first->copyFile("BigFile.txt", "Destination.txt");
            fmt::print("require\n");
            REQUIRE(ret);
        }
        tl::expected<std::string, Ichor::v1::IOError> ret = co_await async_io_svc->first->readWholeFile("Destination.txt");
        REQUIRE(ret);
        REQUIRE((*ret).size() == (uint64_t)bigFileFilesize);
        queue->pushEvent<QuitEvent>(ServiceIdType{0});
        co_return {};
    });

    auto start = std::chrono::steady_clock::now();
    while(queue->is_running()) {
        std::this_thread::sleep_for(10ms);
        auto now = std::chrono::steady_clock::now();
        REQUIRE(now - start < 20s);
    }

    t.join();
}

TEST_CASE_METHOD(AsyncFileIOExpensiveSetup, "AsyncFileIOTests_uring: Removing non-existing file should error") {
    auto version = Ichor::v1::kernelVersion();

    REQUIRE(version);
    if(version < v1::Version{5, 18, 0}) {
        SKIP("Kernel version too old to run test.");
        return;
    }

    auto gen_i = GENERATE(1, 2);

    if(gen_i == 2) {
        emulateKernelVersion = v1::Version{5, 18, 0};
        fmt::println("emulating kernel version {}", *emulateKernelVersion);
    } else {
        fmt::println("kernel version {}", *version);
    }

    fmt::print("section 6\n");
    auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
    auto &dm = queue->createManager();
    ServiceIdType ioSvcId{};

    std::thread t([&]() {
        REQUIRE(queue->createEventLoop());
        dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
        ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
        queue->start(CaptureSigInt);
    });

    waitForRunning(dm);

    runForOrQueueEmpty(dm);

    queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, [&]() -> AsyncGenerator<IchorBehaviour> {
        fmt::print("run function co_await\n");
        auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
        tl::expected<void, Ichor::v1::IOError> ret = co_await async_io_svc->first->removeFile("MissingFile.txt");
        REQUIRE(!ret);
        REQUIRE(ret.error() == IOError::FILE_DOES_NOT_EXIST);
        queue->pushEvent<QuitEvent>(ServiceIdType{0});
        co_return {};
    });

    auto start = std::chrono::steady_clock::now();
    while(queue->is_running()) {
        std::this_thread::sleep_for(10ms);
        auto now = std::chrono::steady_clock::now();
        REQUIRE(now - start < 20s);
    }

    t.join();
}

TEST_CASE_METHOD(AsyncFileIOExpensiveSetup, "AsyncFileIOTests_uring: Removing file") {
    auto version = Ichor::v1::kernelVersion();

    REQUIRE(version);
    if(version < v1::Version{5, 18, 0}) {
        SKIP("Kernel version too old to run test.");
        return;
    }

    auto gen_i = GENERATE(1, 2);

    if(gen_i == 2) {
        emulateKernelVersion = v1::Version{5, 18, 0};
        fmt::println("emulating kernel version {}", *emulateKernelVersion);
    } else {
        fmt::println("kernel version {}", *version);
    }

    fmt::print("section 7\n");
    auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
    auto &dm = queue->createManager();
    ServiceIdType ioSvcId{};

    std::thread t([&]() {
        REQUIRE(queue->createEventLoop());
        dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
        ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
        queue->start(CaptureSigInt);
    });

    {
        std::ofstream out("AsyncFileIO.txt");
        out << "This is a test";
        out.close();
    }

    waitForRunning(dm);

    runForOrQueueEmpty(dm);

    queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, [&]() -> AsyncGenerator<IchorBehaviour> {
        fmt::print("run function co_await\n");
        auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
        {
            tl::expected<void, Ichor::v1::IOError> ret = co_await async_io_svc->first->removeFile("AsyncFileIO.txt");
            fmt::print("require\n");
            REQUIRE(ret);
        }
        tl::expected<std::string, Ichor::v1::IOError> ret = co_await async_io_svc->first->readWholeFile("AsyncFileIO.txt");
        REQUIRE(!ret);
        REQUIRE(ret.error() == IOError::FILE_DOES_NOT_EXIST);
        queue->pushEvent<QuitEvent>(ServiceIdType{0});
        co_return {};
    });

    auto start = std::chrono::steady_clock::now();
    while(queue->is_running()) {
        std::this_thread::sleep_for(10ms);
        auto now = std::chrono::steady_clock::now();
        REQUIRE(now - start < 20s);
    }

    t.join();
}

TEST_CASE_METHOD(AsyncFileIOExpensiveSetup, "AsyncFileIOTests_uring: Writing file") {
    auto version = Ichor::v1::kernelVersion();

    REQUIRE(version);
    if(version < v1::Version{5, 18, 0}) {
        SKIP("Kernel version too old to run test.");
        return;
    }

    auto gen_i = GENERATE(1, 2);

    if(gen_i == 2) {
        emulateKernelVersion = v1::Version{5, 18, 0};
        fmt::println("emulating kernel version {}", *emulateKernelVersion);
    } else {
        fmt::println("kernel version {}", *version);
    }

    fmt::print("section 8\n");
    auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
    auto &dm = queue->createManager();
    ServiceIdType ioSvcId{};

    std::thread t([&]() {
        REQUIRE(queue->createEventLoop());
        dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
        ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
        queue->start(CaptureSigInt);
    });

    {
        std::remove("AsyncFileIO.txt"); // ignore error, each OS handles this differently and most of the time it's just because it didn't exist in the first place
    }

    waitForRunning(dm);

    runForOrQueueEmpty(dm);

    queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, [&]() -> AsyncGenerator<IchorBehaviour> {
        fmt::print("run function co_await\n");
        auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
        {
            tl::expected<void, Ichor::v1::IOError> ret = co_await async_io_svc->first->writeFile("AsyncFileIO.txt", "This is a test");
            fmt::print("require\n");
            REQUIRE(ret);
        }
        tl::expected<std::string, Ichor::v1::IOError> ret = co_await async_io_svc->first->readWholeFile("AsyncFileIO.txt");
        REQUIRE(ret);
        REQUIRE(ret == "This is a test");
        queue->pushEvent<QuitEvent>(ServiceIdType{0});
        co_return {};
    });

    auto start = std::chrono::steady_clock::now();
    while(queue->is_running()) {
        std::this_thread::sleep_for(10ms);
        auto now = std::chrono::steady_clock::now();
        REQUIRE(now - start < 20s);
    }

    t.join();
}

TEST_CASE_METHOD(AsyncFileIOExpensiveSetup, "AsyncFileIOTests_uring: Writing file - overwrite") {
    auto version = Ichor::v1::kernelVersion();

    REQUIRE(version);
    if(version < v1::Version{5, 18, 0}) {
        SKIP("Kernel version too old to run test.");
        return;
    }

    auto gen_i = GENERATE(1, 2);

    if(gen_i == 2) {
        emulateKernelVersion = v1::Version{5, 18, 0};
        fmt::println("emulating kernel version {}", *emulateKernelVersion);
    } else {
        fmt::println("kernel version {}", *version);
    }

    fmt::print("section 9\n");
    auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
    auto &dm = queue->createManager();
    ServiceIdType ioSvcId{};

    std::thread t([&]() {
        REQUIRE(queue->createEventLoop());
        dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
        ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
        queue->start(CaptureSigInt);
    });

    {
        std::ofstream out("AsyncFileIO.txt");
        out << "This is a test";
        out.close();
    }

    waitForRunning(dm);

    runForOrQueueEmpty(dm);

    queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, [&]() -> AsyncGenerator<IchorBehaviour> {
        fmt::print("run function co_await\n");
        auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
        tl::expected<std::string, Ichor::v1::IOError> ret = co_await async_io_svc->first->readWholeFile("AsyncFileIO.txt");
        REQUIRE(ret);
        {
            tl::expected<void, Ichor::v1::IOError> ret2 = co_await async_io_svc->first->writeFile("AsyncFileIO.txt", "Overwrite");
            fmt::print("require\n");
            REQUIRE(ret2);
        }
        ret = co_await async_io_svc->first->readWholeFile("AsyncFileIO.txt");
        REQUIRE(ret);
        REQUIRE(ret == "Overwrite");
        queue->pushEvent<QuitEvent>(ServiceIdType{0});
        co_return {};
    });

    auto start = std::chrono::steady_clock::now();
    while(queue->is_running()) {
        std::this_thread::sleep_for(10ms);
        auto now = std::chrono::steady_clock::now();
        REQUIRE(now - start < 20s);
    }

    t.join();
}

TEST_CASE_METHOD(AsyncFileIOExpensiveSetup, "AsyncFileIOTests_uring: appending file") {
    auto version = Ichor::v1::kernelVersion();

    REQUIRE(version);
    if(version < v1::Version{5, 18, 0}) {
        SKIP("Kernel version too old to run test.");
        return;
    }

    auto gen_i = GENERATE(1, 2);

    if(gen_i == 2) {
        emulateKernelVersion = v1::Version{5, 18, 0};
        fmt::println("emulating kernel version {}", *emulateKernelVersion);
    } else {
        fmt::println("kernel version {}", *version);
    }

    fmt::print("section 10\n");
    auto queue = std::make_unique<QIMPL>(500, 100'000'000, emulateKernelVersion);
    auto &dm = queue->createManager();
    ServiceIdType ioSvcId{};

    std::thread t([&]() {
        REQUIRE(queue->createEventLoop());
        dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::v1::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
        ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
        queue->start(CaptureSigInt);
    });

    {
        std::ofstream out("AsyncFileIO.txt");
        out << "This is a test";
        out.close();
    }

    waitForRunning(dm);

    runForOrQueueEmpty(dm);

    queue->pushEvent<RunFunctionEventAsync>(ServiceIdType{0}, [&]() -> AsyncGenerator<IchorBehaviour> {
        fmt::print("run function co_await\n");
        auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
        tl::expected<std::string, Ichor::v1::IOError> ret = co_await async_io_svc->first->readWholeFile("AsyncFileIO.txt");
        REQUIRE(ret);
        {
            tl::expected<void, Ichor::v1::IOError> ret2 = co_await async_io_svc->first->appendFile("AsyncFileIO.txt", "Overwrite");
            fmt::print("require\n");
            REQUIRE(ret2);
        }
        ret = co_await async_io_svc->first->readWholeFile("AsyncFileIO.txt");
        REQUIRE(ret);
        REQUIRE(ret == "This is a testOverwrite");
        queue->pushEvent<QuitEvent>(ServiceIdType{0});
        co_return {};
    });

    auto start = std::chrono::steady_clock::now();
    while(queue->is_running()) {
        std::this_thread::sleep_for(10ms);
        auto now = std::chrono::steady_clock::now();
        REQUIRE(now - start < 20s);
    }

    t.join();
}
