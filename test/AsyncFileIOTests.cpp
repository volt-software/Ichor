#include "Common.h"
#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/logging/CoutLogger.h>
#include <fstream>
#include <filesystem>

#ifdef TEST_URING
#include <ichor/event_queues/IOUringQueue.h>
#include <ichor/services/io/IOUringAsyncFileIO.h>

#define IOIMPL IOUringAsyncFileIO
#define QIMPL IOUringQueue
#else
#include <ichor/services/io/SharedOverThreadsAsyncFileIO.h>

#define IOIMPL SharedOverThreadsAsyncFileIO
#define QIMPL PriorityQueue
#endif

struct AsyncFileIOExpensiveSetup {
    AsyncFileIOExpensiveSetup() {
        std::ofstream out("BigFile.txt");
        while(out.tellp() < 8192*1024) {
            REQUIRE(!out.fail());
            out << "This is a test123";
        }
        REQUIRE(!out.fail());
        bigFilefilesize = out.tellp();
        REQUIRE(bigFilefilesize >= 0);
        out.close();
    }

    ~AsyncFileIOExpensiveSetup() noexcept {
        std::remove("BigFile.txt"); // ignore error, each OS handles this differently and most of the time it's just because it didn't exist in the first place
    }

    int64_t bigFilefilesize{};
};

#ifdef TEST_URING
TEST_CASE_METHOD(AsyncFileIOExpensiveSetup, "AsyncFileIOTests_uring") {
#else
TEST_CASE_METHOD(AsyncFileIOExpensiveSetup, "AsyncFileIOTests") {
#endif

    SECTION("Reading non-existent file should error") {
        fmt::print("section 1\n");
        auto queue = std::make_unique<QIMPL>();
        auto &dm = queue->createManager();
        uint64_t ioSvcId{};

        std::thread t([&]() {
#ifdef TEST_URING
            queue->createEventLoop();
#endif
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
            ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            fmt::print("run function co_await\n");
            auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
            tl::expected<std::string, Ichor::FileIOError> ret = co_await async_io_svc->first->readWholeFile("NonExistentFile.txt");
            if(!ret) {
                fmt::println("readWholeFile error {}", ret.error());
            } else {
                fmt::println("readWholeFile found an existing file.");
            }
            REQUIRE(!ret);
            REQUIRE(ret.error() == FileIOError::FILE_DOES_NOT_EXIST);
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }

    SECTION("Reading file without permissions should error") {
        fmt::print("section 1\n");
        auto queue = std::make_unique<QIMPL>();
        auto &dm = queue->createManager();
        uint64_t ioSvcId{};

        std::thread t([&]() {
#ifdef TEST_URING
            queue->createEventLoop();
#endif
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
            ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
            queue->start(CaptureSigInt);
        });

        {
            std::remove("NoPermIO.txt"); // ignore error, each OS handles this differently and most of the time it's just because it didn't exist in the first place

            std::ofstream out("NoPermIO.txt");
            out << "This is a test";
            out.close();

            std::error_code ec;
            std::filesystem::permissions("NoPermIO.txt", std::filesystem::perms::none, ec);
            REQUIRE(!ec);
        }

        waitForRunning(dm);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            fmt::print("run function co_await\n");
            auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
            tl::expected<std::string, Ichor::FileIOError> ret = co_await async_io_svc->first->readWholeFile("NoPermIO.txt");
            if(!ret) {
                fmt::println("readWholeFile error {}", ret.error());
            } else {
                fmt::println("readWholeFile found an existing file. {}", (*ret).size());
            }
            REQUIRE(!ret);
            REQUIRE(ret.error() == FileIOError::NO_PERMISSION);
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();

        std::remove("NoPermIO.txt"); // ignore error, each OS handles this differently and most of the time it's just because it didn't exist in the first place
    }

    SECTION("Read whole file Small") {
        fmt::print("section 2a\n");
        auto queue = std::make_unique<QIMPL>();
        auto &dm = queue->createManager();
        uint64_t ioSvcId{};

        std::thread t([&]() {
#ifdef TEST_URING
            queue->createEventLoop();
#endif
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
            ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
            queue->start(CaptureSigInt);
        });

        {
            std::ofstream out("AsyncFileIO.txt");
            out << "This is a test";
            out.close();
        }

        waitForRunning(dm);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            fmt::print("run function co_await\n");
            auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
            tl::expected<std::string, Ichor::FileIOError> ret = co_await async_io_svc->first->readWholeFile("AsyncFileIO.txt");
            fmt::print("require\n");
            REQUIRE(ret == "This is a test");
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }
    SECTION("Read whole file large") {
        fmt::println("section 2b");
        auto queue = std::make_unique<QIMPL>();
        auto &dm = queue->createManager();
        uint64_t ioSvcId{};

        std::thread t([&]() {
#ifdef TEST_URING
            queue->createEventLoop();
#endif
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
            ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            fmt::println("run function co_await");
            auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
            tl::expected<std::string, Ichor::FileIOError> ret = co_await async_io_svc->first->readWholeFile("BigFile.txt");
            if(!ret) {
                fmt::println("readWholeFile error {}\n", ret.error());
            } else {
                fmt::println("readWholeFile found an existing file.");
            }
            REQUIRE(ret);
            REQUIRE((*ret).size() == (uint64_t)bigFilefilesize);
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }

    SECTION("Copying non-existent file should error") {
        fmt::print("section 3\n");
        auto queue = std::make_unique<QIMPL>();
        auto &dm = queue->createManager();
        uint64_t ioSvcId{};

        std::thread t([&]() {
#ifdef TEST_URING
            queue->createEventLoop();
#endif
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
            ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            fmt::print("run function co_await\n");
            auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
            tl::expected<void, Ichor::FileIOError> ret = co_await async_io_svc->first->copyFile("NonExistentFile.txt", "DestinationNull.txt");
            fmt::print("require\n");
            REQUIRE(!ret);
            REQUIRE(ret.error() == FileIOError::FILE_DOES_NOT_EXIST);
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }

    SECTION("Copying file") {
        fmt::print("section 4\n");
        auto queue = std::make_unique<QIMPL>();
        auto &dm = queue->createManager();
        uint64_t ioSvcId{};

        std::thread t([&]() {
#ifdef TEST_URING
            queue->createEventLoop();
#endif
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
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

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            fmt::print("run function co_await\n");
            auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
            {
                tl::expected<void, Ichor::FileIOError> ret = co_await async_io_svc->first->copyFile("AsyncFileIO.txt", "Destination.txt");
                fmt::print("require\n");
                REQUIRE(ret);
            }
            tl::expected<std::string, Ichor::FileIOError> ret = co_await async_io_svc->first->readWholeFile("Destination.txt");
            REQUIRE(ret == "This is a test");
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }

    SECTION("Copying large file") {
        fmt::print("section 5\n");
        auto queue = std::make_unique<QIMPL>();
        auto &dm = queue->createManager();
        uint64_t ioSvcId{};

        std::thread t([&]() {
#ifdef TEST_URING
            queue->createEventLoop();
#endif
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
            ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
            queue->start(CaptureSigInt);
        });

        std::remove("Destination.txt"); // ignore error, each OS handles this differently and most of the time it's just because it didn't exist in the first place

        waitForRunning(dm);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            fmt::print("run function co_await\n");
            auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
            {
                tl::expected<void, Ichor::FileIOError> ret = co_await async_io_svc->first->copyFile("BigFile.txt", "Destination.txt");
                fmt::print("require\n");
                REQUIRE(ret);
            }
            tl::expected<std::string, Ichor::FileIOError> ret = co_await async_io_svc->first->readWholeFile("Destination.txt");
            REQUIRE(ret);
            REQUIRE((*ret).size() == (uint64_t)bigFilefilesize);
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }

    SECTION("Removing non-existing file should error") {
        fmt::print("section 6\n");
        auto queue = std::make_unique<QIMPL>();
        auto &dm = queue->createManager();
        uint64_t ioSvcId{};

        std::thread t([&]() {
#ifdef TEST_URING
            queue->createEventLoop();
#endif
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
            ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            fmt::print("run function co_await\n");
            auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
            tl::expected<void, Ichor::FileIOError> ret = co_await async_io_svc->first->removeFile("MissingFile.txt");
            REQUIRE(!ret);
            REQUIRE(ret.error() == FileIOError::FILE_DOES_NOT_EXIST);
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }

    SECTION("Removing file") {
        fmt::print("section 7\n");
        auto queue = std::make_unique<QIMPL>();
        auto &dm = queue->createManager();
        uint64_t ioSvcId{};

        std::thread t([&]() {
#ifdef TEST_URING
            queue->createEventLoop();
#endif
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
            ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
            queue->start(CaptureSigInt);
        });

        {
            std::ofstream out("AsyncFileIO.txt");
            out << "This is a test";
            out.close();
        }

        waitForRunning(dm);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            fmt::print("run function co_await\n");
            auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
            {
                tl::expected<void, Ichor::FileIOError> ret = co_await async_io_svc->first->removeFile("AsyncFileIO.txt");
                fmt::print("require\n");
                REQUIRE(ret);
            }
            tl::expected<std::string, Ichor::FileIOError> ret = co_await async_io_svc->first->readWholeFile("AsyncFileIO.txt");
            REQUIRE(!ret);
            REQUIRE(ret.error() == FileIOError::FILE_DOES_NOT_EXIST);
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }

    SECTION("Writing file") {
        fmt::print("section 8\n");
        auto queue = std::make_unique<QIMPL>();
        auto &dm = queue->createManager();
        uint64_t ioSvcId{};

        std::thread t([&]() {
#ifdef TEST_URING
            queue->createEventLoop();
#endif
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
            ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
            queue->start(CaptureSigInt);
        });

        {
            std::remove("AsyncFileIO.txt"); // ignore error, each OS handles this differently and most of the time it's just because it didn't exist in the first place
        }

        waitForRunning(dm);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            fmt::print("run function co_await\n");
            auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
            {
                tl::expected<void, Ichor::FileIOError> ret = co_await async_io_svc->first->writeFile("AsyncFileIO.txt", "This is a test");
                fmt::print("require\n");
                REQUIRE(ret);
            }
            tl::expected<std::string, Ichor::FileIOError> ret = co_await async_io_svc->first->readWholeFile("AsyncFileIO.txt");
            REQUIRE(ret);
            REQUIRE(ret == "This is a test");
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }

    SECTION("Writing file - overwrite") {
        fmt::print("section 9\n");
        auto queue = std::make_unique<QIMPL>();
        auto &dm = queue->createManager();
        uint64_t ioSvcId{};

        std::thread t([&]() {
#ifdef TEST_URING
            queue->createEventLoop();
#endif
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
            ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
            queue->start(CaptureSigInt);
        });

        {
            std::ofstream out("AsyncFileIO.txt");
            out << "This is a test";
            out.close();
        }

        waitForRunning(dm);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            fmt::print("run function co_await\n");
            auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
            tl::expected<std::string, Ichor::FileIOError> ret = co_await async_io_svc->first->readWholeFile("AsyncFileIO.txt");
            REQUIRE(ret);
            {
                tl::expected<void, Ichor::FileIOError> ret2 = co_await async_io_svc->first->writeFile("AsyncFileIO.txt", "Overwrite");
                fmt::print("require\n");
                REQUIRE(ret2);
            }
            ret = co_await async_io_svc->first->readWholeFile("AsyncFileIO.txt");
            REQUIRE(ret);
            REQUIRE(ret == "Overwrite");
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }

    SECTION("Appending file") {
        fmt::print("section 10\n");
        auto queue = std::make_unique<QIMPL>();
        auto &dm = queue->createManager();
        uint64_t ioSvcId{};

        std::thread t([&]() {
#ifdef TEST_URING
            queue->createEventLoop();
#endif
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}});
            ioSvcId = dm.createServiceManager<IOIMPL, IAsyncFileIO>()->getServiceId();
            queue->start(CaptureSigInt);
        });

        {
            std::ofstream out("AsyncFileIO.txt");
            out << "This is a test";
            out.close();
        }

        waitForRunning(dm);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            fmt::print("run function co_await\n");
            auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
            tl::expected<std::string, Ichor::FileIOError> ret = co_await async_io_svc->first->readWholeFile("AsyncFileIO.txt");
            REQUIRE(ret);
            {
                tl::expected<void, Ichor::FileIOError> ret2 = co_await async_io_svc->first->appendFile("AsyncFileIO.txt", "Overwrite");
                fmt::print("require\n");
                REQUIRE(ret2);
            }
            ret = co_await async_io_svc->first->readWholeFile("AsyncFileIO.txt");
            REQUIRE(ret);
            REQUIRE(ret == "This is a testOverwrite");
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }
}
