#include "Common.h"
#include <ichor/event_queues/PriorityQueue.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/services/io/SharedOverThreadsAsyncFileIO.h>
#include <fstream>
#include <filesystem>


TEST_CASE("AsyncFileIOTests") {

    SECTION("Reading non-existent file should error") {
        fmt::print("section 1\n");
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        uint64_t ioSvcId{};

        std::thread t([&]() {
            ioSvcId = dm.createServiceManager<SharedOverThreadsAsyncFileIO, IAsyncFileIO>()->getServiceId();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            fmt::print("run function co_await\n");
            auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
            tl::expected<std::string, Ichor::FileIOError> ret = co_await async_io_svc->first->read_whole_file("NonExistentFile.txt");
            fmt::print("require\n");
            REQUIRE(!ret);
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }

    SECTION("Read whole file") {
        fmt::print("section 2\n");
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        uint64_t ioSvcId{};

        std::thread t([&]() {
            ioSvcId = dm.createServiceManager<SharedOverThreadsAsyncFileIO, IAsyncFileIO>()->getServiceId();
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
            tl::expected<std::string, Ichor::FileIOError> ret = co_await async_io_svc->first->read_whole_file("AsyncFileIO.txt");
            fmt::print("require\n");
            REQUIRE(ret == "This is a test");
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }

    SECTION("Copying non-existent file should error") {
        fmt::print("section 3\n");
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        uint64_t ioSvcId{};

        std::thread t([&]() {
            ioSvcId = dm.createServiceManager<SharedOverThreadsAsyncFileIO, IAsyncFileIO>()->getServiceId();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            fmt::print("run function co_await\n");
            auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
            tl::expected<void, Ichor::FileIOError> ret = co_await async_io_svc->first->copy_file("NonExistentFile.txt", "DestinationNull.txt");
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
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        uint64_t ioSvcId{};

        std::thread t([&]() {
            ioSvcId = dm.createServiceManager<SharedOverThreadsAsyncFileIO, IAsyncFileIO>()->getServiceId();
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
                tl::expected<void, Ichor::FileIOError> ret = co_await async_io_svc->first->copy_file("AsyncFileIO.txt", "Destination.txt");
                fmt::print("require\n");
                REQUIRE(ret);
            }
            tl::expected<std::string, Ichor::FileIOError> ret = co_await async_io_svc->first->read_whole_file("AsyncFileIO.txt");
            REQUIRE(ret == "This is a test");
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }

    SECTION("Removing non-existing file should error") {
        fmt::print("section 5\n");
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        uint64_t ioSvcId{};

        std::thread t([&]() {
            ioSvcId = dm.createServiceManager<SharedOverThreadsAsyncFileIO, IAsyncFileIO>()->getServiceId();
            queue->start(CaptureSigInt);
        });

        waitForRunning(dm);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            fmt::print("run function co_await\n");
            auto async_io_svc = dm.getService<IAsyncFileIO>(ioSvcId);
            tl::expected<void, Ichor::FileIOError> ret = co_await async_io_svc->first->remove_file("MissingFile.txt");
            REQUIRE(!ret);
            REQUIRE(ret.error() == FileIOError::FILE_DOES_NOT_EXIST);
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }

    SECTION("Removing file") {
        fmt::print("section 6\n");
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        uint64_t ioSvcId{};

        std::thread t([&]() {
            ioSvcId = dm.createServiceManager<SharedOverThreadsAsyncFileIO, IAsyncFileIO>()->getServiceId();
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
                tl::expected<void, Ichor::FileIOError> ret = co_await async_io_svc->first->remove_file("AsyncFileIO.txt");
                fmt::print("require\n");
                REQUIRE(ret);
            }
            tl::expected<std::string, Ichor::FileIOError> ret = co_await async_io_svc->first->read_whole_file("AsyncFileIO.txt");
            REQUIRE(!ret);
            REQUIRE(ret.error() == FileIOError::FILE_DOES_NOT_EXIST);
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }

    SECTION("Writing file") {
        fmt::print("section 7\n");
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        uint64_t ioSvcId{};

        std::thread t([&]() {
            ioSvcId = dm.createServiceManager<SharedOverThreadsAsyncFileIO, IAsyncFileIO>()->getServiceId();
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
                tl::expected<void, Ichor::FileIOError> ret = co_await async_io_svc->first->write_file("AsyncFileIO.txt", "This is a test");
                fmt::print("require\n");
                REQUIRE(ret);
            }
            tl::expected<std::string, Ichor::FileIOError> ret = co_await async_io_svc->first->read_whole_file("AsyncFileIO.txt");
            REQUIRE(ret);
            REQUIRE(ret == "This is a test");
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }

    SECTION("Writing file - overwrite") {
        fmt::print("section 8\n");
        auto queue = std::make_unique<PriorityQueue>();
        auto &dm = queue->createManager();
        uint64_t ioSvcId{};

        std::thread t([&]() {
            ioSvcId = dm.createServiceManager<SharedOverThreadsAsyncFileIO, IAsyncFileIO>()->getServiceId();
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
            tl::expected<std::string, Ichor::FileIOError> ret = co_await async_io_svc->first->read_whole_file("AsyncFileIO.txt");
            REQUIRE(ret);
            {
                tl::expected<void, Ichor::FileIOError> ret2 = co_await async_io_svc->first->write_file("AsyncFileIO.txt", "Overwrite");
                fmt::print("require\n");
                REQUIRE(ret2);
            }
            ret = co_await async_io_svc->first->read_whole_file("AsyncFileIO.txt");
            REQUIRE(ret);
            REQUIRE(ret == "Overwrite");
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }
}
