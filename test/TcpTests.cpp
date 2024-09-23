#include <ichor/events/RunFunctionEvent.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/network/ClientFactory.h>
#include <ichor/services/logging/CoutFrameworkLogger.h>
#include <ichor/services/logging/CoutLogger.h>
#include "Common.h"
#include "TestServices/TcpService.h"

using namespace Ichor;

std::unique_ptr<Ichor::AsyncManualResetEvent> _evt;
std::atomic<uint64_t> evtGate;

#ifdef TEST_URING
#include <ichor/services/network/tcp/IOUringTcpConnectionService.h>
#include <ichor/services/network/tcp/IOUringTcpHostService.h>
#include <ichor/event_queues/IOUringQueue.h>

#define QIMPL IOUringQueue
#define CONNIMPL IOUringTcpConnectionService
#define HOSTIMPL IOUringTcpHostService
#else
#include <ichor/services/timer/TimerFactoryFactory.h>
#include <ichor/services/network/tcp/TcpConnectionService.h>
#include <ichor/services/network/tcp/TcpHostService.h>
#include <ichor/event_queues/PriorityQueue.h>

#define QIMPL PriorityQueue
#define CONNIMPL TcpConnectionService
#define HOSTIMPL TcpHostService
#endif

using namespace std::string_literals;

#ifdef TEST_URING
TEST_CASE("TcpTests_uring") {
#else
TEST_CASE("TcpTests") {
#endif
    SECTION("Send message") {
        _evt = std::make_unique<Ichor::AsyncManualResetEvent>();
        auto queue = std::make_unique<QIMPL>(true);
        ServiceIdType tcpClientId;
        evtGate = false;

        std::thread t([&]() {
#ifdef TEST_URING
            REQUIRE(queue->createEventLoop());
#endif
            auto &dm = queue->createManager();
            uint64_t priorityToEnsureHostStartingFirst = 51;
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}}, priorityToEnsureHostStartingFirst);
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}}, priorityToEnsureHostStartingFirst);
            dm.createServiceManager<HOSTIMPL, IHostService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1"s)}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}}, priorityToEnsureHostStartingFirst);
            dm.createServiceManager<ClientFactory<CONNIMPL>, IClientFactory>();
#ifndef TEST_URING
            dm.createServiceManager<TimerFactoryFactory>(Properties{}, priorityToEnsureHostStartingFirst);
#endif
            tcpClientId = dm.createServiceManager<TcpService, ITcpService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1"s)}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}})->getServiceId();

            queue->start(CaptureSigInt);
        });

        auto start = std::chrono::steady_clock::now();
        while(evtGate.load(std::memory_order_acquire) != 1) {
            std::this_thread::sleep_for(500us);
            auto now = std::chrono::steady_clock::now();
            REQUIRE(now - start < 1s);
        }

        evtGate.store(0, std::memory_order_release);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            auto &dm = GetThreadLocalManager();
            auto svc = dm.getService<ITcpService>(tcpClientId);
            REQUIRE(svc);
            std::vector<uint8_t> data;
            std::string_view str = "This is a message\n";
            data.assign(str.begin(), str.end());
            auto ret = co_await (*svc).first->sendClientAsync(std::move(data));
            REQUIRE(ret);
            co_return {};
        });

        start = std::chrono::steady_clock::now();
        while(evtGate.load(std::memory_order_acquire) != 1) {
            std::this_thread::sleep_for(500us);
            auto now = std::chrono::steady_clock::now();
            REQUIRE(now - start < 1s);
        }

        evtGate.store(0, std::memory_order_release);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            auto &dm = GetThreadLocalManager();
            auto svc = dm.getService<ITcpService>(tcpClientId);
            REQUIRE(svc);
            auto& msgs = (*svc).first->getMsgs();
            REQUIRE(msgs.size() == 2);
            std::span<uint8_t> msg{msgs[0].begin(), msgs[0].end()};
            std::string_view str{reinterpret_cast<char*>(msg.data()), msg.size()};
            REQUIRE(str == "This is a message\n");

            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }

    SECTION("Send huge message") {
        _evt = std::make_unique<Ichor::AsyncManualResetEvent>();
        auto queue = std::make_unique<QIMPL>(true);
        ServiceIdType tcpClientId;
        evtGate = false;

        std::thread t([&]() {
#ifdef TEST_URING
            REQUIRE(queue->createEventLoop());
#endif
            auto &dm = queue->createManager();
            uint64_t priorityToEnsureHostStartingFirst = 51;
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}}, priorityToEnsureHostStartingFirst);
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}}, priorityToEnsureHostStartingFirst);
            dm.createServiceManager<HOSTIMPL, IHostService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1"s)}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}, {"BufferEntries", Ichor::make_any<uint32_t>(static_cast<uint16_t>(16))}, {"BufferEntrySize", Ichor::make_any<uint32_t>(static_cast<uint16_t>(16'384))}}, priorityToEnsureHostStartingFirst);
            dm.createServiceManager<ClientFactory<CONNIMPL>, IClientFactory>();
#ifndef TEST_URING
            dm.createServiceManager<TimerFactoryFactory>(Properties{}, priorityToEnsureHostStartingFirst);
#endif
            tcpClientId = dm.createServiceManager<TcpService, ITcpService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1"s)}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}})->getServiceId();

            queue->start(CaptureSigInt);
        });
        std::vector<uint8_t> data;
        for(uint64_t i = 0; i < 256'000; i++) {
            data.emplace_back('a');
        }
        data.emplace_back('\n');

        auto start = std::chrono::steady_clock::now();
        while(evtGate.load(std::memory_order_acquire) != 1) {
            std::this_thread::sleep_for(500us);
            auto now = std::chrono::steady_clock::now();
            REQUIRE(now - start < 1s);
        }

        evtGate.store(0, std::memory_order_release);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            auto &dm = GetThreadLocalManager();
            auto svc = dm.getService<ITcpService>(tcpClientId);
            REQUIRE(svc);
            auto ret = co_await (*svc).first->sendClientAsync(std::move(data));
            REQUIRE(ret);
            co_return {};
        });

        start = std::chrono::steady_clock::now();
        while(evtGate.load(std::memory_order_acquire) != 1) {
            std::this_thread::sleep_for(500us);
            auto now = std::chrono::steady_clock::now();
            REQUIRE(now - start < 1s);
        }

        evtGate.store(0, std::memory_order_release);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            auto &dm = GetThreadLocalManager();
            auto svc = dm.getService<ITcpService>(tcpClientId);
            REQUIRE(svc);
            auto& msgs = (*svc).first->getMsgs();
            REQUIRE(msgs.size() == 2);
            REQUIRE(msgs[0] == data);

            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }

    SECTION("Send multiple message") {
        _evt = std::make_unique<Ichor::AsyncManualResetEvent>();
        auto queue = std::make_unique<QIMPL>(true);
        ServiceIdType tcpClientId;
        evtGate = false;

        std::thread t([&]() {
#ifdef TEST_URING
            REQUIRE(queue->createEventLoop());
#endif
            auto &dm = queue->createManager();
            uint64_t priorityToEnsureHostStartingFirst = 51;
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}}, priorityToEnsureHostStartingFirst);
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}}, priorityToEnsureHostStartingFirst);
            dm.createServiceManager<HOSTIMPL, IHostService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1"s)}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}}, priorityToEnsureHostStartingFirst);
            dm.createServiceManager<ClientFactory<CONNIMPL>, IClientFactory>();
#ifndef TEST_URING
            dm.createServiceManager<TimerFactoryFactory>(Properties{}, priorityToEnsureHostStartingFirst);
#endif
            tcpClientId = dm.createServiceManager<TcpService, ITcpService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1"s)}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}})->getServiceId();

            queue->start(CaptureSigInt);
        });

        auto start = std::chrono::steady_clock::now();
        while(evtGate.load(std::memory_order_acquire) != 1) {
            std::this_thread::sleep_for(500us);
            auto now = std::chrono::steady_clock::now();
            REQUIRE(now - start < 1s);
        }

        evtGate.store(0, std::memory_order_release);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            auto &dm = GetThreadLocalManager();
            auto svc = dm.getService<ITcpService>(tcpClientId);
            REQUIRE(svc);
            {
                std::vector<uint8_t> data;
                std::string_view str = "This is a message\n";
                data.assign(str.begin(), str.end());
                auto ret = co_await (*svc).first->sendClientAsync(std::move(data));
                REQUIRE(ret);
            }
            {
                std::vector<uint8_t> data;
                std::string_view str = "This is a message\n";
                data.assign(str.begin(), str.end());
                auto ret = co_await (*svc).first->sendClientAsync(std::move(data));
                REQUIRE(ret);
            }
            co_return {};
        });

        start = std::chrono::steady_clock::now();
        while(evtGate.load(std::memory_order_acquire) != 2) {
            std::this_thread::sleep_for(500us);
            auto now = std::chrono::steady_clock::now();
            REQUIRE(now - start < 1s);
        }

        evtGate.store(0, std::memory_order_release);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            auto &dm = GetThreadLocalManager();
            auto svc = dm.getService<ITcpService>(tcpClientId);
            REQUIRE(svc);
            auto& msgs = (*svc).first->getMsgs();
            REQUIRE(msgs.size() == 3);
            {
                std::span<uint8_t> msg{msgs[0].begin(), msgs[0].end()};
                std::string_view str{reinterpret_cast<char *>(msg.data()), msg.size()};
                REQUIRE(str == "This is a message\n");
            }
            {
                std::span<uint8_t> msg{msgs[1].begin(), msgs[1].end()};
                std::string_view str{reinterpret_cast<char *>(msg.data()), msg.size()};
                REQUIRE(str == "This is a message\n");
            }

            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }

    SECTION("Send multiple message in one go") {
        _evt = std::make_unique<Ichor::AsyncManualResetEvent>();
        auto queue = std::make_unique<QIMPL>(true);
        ServiceIdType tcpClientId;
        evtGate = false;

        std::thread t([&]() {
#ifdef TEST_URING
            REQUIRE(queue->createEventLoop());
#endif
            auto &dm = queue->createManager();
            uint64_t priorityToEnsureHostStartingFirst = 51;
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}}, priorityToEnsureHostStartingFirst);
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}}, priorityToEnsureHostStartingFirst);
            dm.createServiceManager<HOSTIMPL, IHostService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1"s)}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}}, priorityToEnsureHostStartingFirst);
            dm.createServiceManager<ClientFactory<CONNIMPL>, IClientFactory>();
#ifndef TEST_URING
            dm.createServiceManager<TimerFactoryFactory>(Properties{}, priorityToEnsureHostStartingFirst);
#endif
            tcpClientId = dm.createServiceManager<TcpService, ITcpService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1"s)}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}})->getServiceId();

            queue->start(CaptureSigInt);
        });

        auto start = std::chrono::steady_clock::now();
        while(evtGate.load(std::memory_order_acquire) != 1) {
            std::this_thread::sleep_for(500us);
            auto now = std::chrono::steady_clock::now();
            REQUIRE(now - start < 1s);
        }

        evtGate.store(0, std::memory_order_release);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            auto &dm = GetThreadLocalManager();
            auto svc = dm.getService<ITcpService>(tcpClientId);
            REQUIRE(svc);
            std::vector<std::vector<uint8_t>> data;
            std::vector<uint8_t> subdata;
            std::string_view str = "This is a message";
            subdata.assign(str.begin(), str.end());
            data.push_back(subdata);
            str = "This is a message\n";
            subdata.assign(str.begin(), str.end());
            data.push_back(subdata);
            auto ret = co_await (*svc).first->sendClientAsync(std::move(data));
            REQUIRE(ret);
            co_return {};
        });

        start = std::chrono::steady_clock::now();
        while(evtGate.load(std::memory_order_acquire) != 1) {
            std::this_thread::sleep_for(500us);
            auto now = std::chrono::steady_clock::now();
            REQUIRE(now - start < 1s);
        }

        evtGate.store(0, std::memory_order_release);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            auto &dm = GetThreadLocalManager();
            auto svc = dm.getService<ITcpService>(tcpClientId);
            REQUIRE(svc);
            auto& msgs = (*svc).first->getMsgs();
            REQUIRE(msgs.size() == 2);
            std::span<uint8_t> msg{msgs[0].begin(), msgs[0].end()};
            std::string_view str{reinterpret_cast<char *>(msg.data()), msg.size()};
            REQUIRE(str == "This is a messageThis is a message\n");

            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }

    SECTION("Send multiple message in one go huge") {
        _evt = std::make_unique<Ichor::AsyncManualResetEvent>();
        auto queue = std::make_unique<QIMPL>(true);
        ServiceIdType tcpClientId;
        evtGate = false;

        std::thread t([&]() {
#ifdef TEST_URING
            REQUIRE(queue->createEventLoop());
#endif
            auto &dm = queue->createManager();
            uint64_t priorityToEnsureHostStartingFirst = 51;
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}}, priorityToEnsureHostStartingFirst);
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}}, priorityToEnsureHostStartingFirst);
            dm.createServiceManager<HOSTIMPL, IHostService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1"s)}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}, {"BufferEntries", Ichor::make_any<uint32_t>(static_cast<uint16_t>(512))}, {"BufferEntrySize", Ichor::make_any<uint32_t>(static_cast<uint16_t>(32))}}, priorityToEnsureHostStartingFirst);
            dm.createServiceManager<ClientFactory<CONNIMPL>, IClientFactory>();
#ifndef TEST_URING
            dm.createServiceManager<TimerFactoryFactory>(Properties{}, priorityToEnsureHostStartingFirst);
#endif
            tcpClientId = dm.createServiceManager<TcpService, ITcpService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1"s)}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}})->getServiceId();

            queue->start(CaptureSigInt);
        });

        auto start = std::chrono::steady_clock::now();
        while(evtGate.load(std::memory_order_acquire) != 1) {
            std::this_thread::sleep_for(500us);
            auto now = std::chrono::steady_clock::now();
            REQUIRE(now - start < 1s);
        }

        evtGate.store(0, std::memory_order_release);
#define TEST_SIZE 128'00

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            auto &dm = GetThreadLocalManager();
            auto svc = dm.getService<ITcpService>(tcpClientId);
            REQUIRE(svc);
            std::vector<std::vector<uint8_t>> data;
            std::vector<uint8_t> subdata;
            std::string_view str = "This is a message";
            subdata.assign(str.begin(), str.end());
            while(data.size() * str.size() < TEST_SIZE) {
                data.push_back(subdata);
            }
            str = "This is a message\n";
            subdata.assign(str.begin(), str.end());
            data.push_back(subdata);
            auto ret = co_await (*svc).first->sendClientAsync(std::move(data));
            REQUIRE(ret);
            co_return {};
        });

        start = std::chrono::steady_clock::now();
        while(evtGate.load(std::memory_order_acquire) != 1) {
            std::this_thread::sleep_for(500us);
            auto now = std::chrono::steady_clock::now();
            REQUIRE(now - start < 1s);
        }

        evtGate.store(0, std::memory_order_release);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            auto &dm = GetThreadLocalManager();
            auto svc = dm.getService<ITcpService>(tcpClientId);
            REQUIRE(svc);
            auto& msgs = (*svc).first->getMsgs();
            REQUIRE(msgs.size() == 2);
            REQUIRE(msgs[0].size() > TEST_SIZE);
            REQUIRE(msgs[0].back() == '\n');

            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }

    SECTION("Closing connection removes services") {
        _evt = std::make_unique<Ichor::AsyncManualResetEvent>();
        auto queue = std::make_unique<QIMPL>(true);
        ServiceIdType tcpClientId;
        evtGate = false;

        std::thread t([&]() {
#ifdef TEST_URING
            REQUIRE(queue->createEventLoop());
#endif
            auto &dm = queue->createManager();
            uint64_t priorityToEnsureHostStartingFirst = 51;
            dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}}, priorityToEnsureHostStartingFirst);
            dm.createServiceManager<LoggerFactory<CoutLogger>, ILoggerFactory>(Properties{{"DefaultLogLevel", Ichor::make_any<LogLevel>(LogLevel::LOG_TRACE)}}, priorityToEnsureHostStartingFirst);
            dm.createServiceManager<HOSTIMPL, IHostService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1"s)}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}}, priorityToEnsureHostStartingFirst);
            dm.createServiceManager<ClientFactory<CONNIMPL>, IClientFactory>();
#ifndef TEST_URING
            dm.createServiceManager<TimerFactoryFactory>(Properties{}, priorityToEnsureHostStartingFirst);
#endif
            tcpClientId = dm.createServiceManager<TcpService, ITcpService>(Properties{{"Address", Ichor::make_any<std::string>("127.0.0.1"s)}, {"Port", Ichor::make_any<uint16_t>(static_cast<uint16_t>(8001))}})->getServiceId();

            queue->start(CaptureSigInt);
        });

        auto start = std::chrono::steady_clock::now();
        while(evtGate.load(std::memory_order_acquire) != 1) {
            std::this_thread::sleep_for(500us);
            auto now = std::chrono::steady_clock::now();
            REQUIRE(now - start < 1s);
        }

        evtGate.store(0, std::memory_order_release);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            auto &dm = GetThreadLocalManager();
            auto svc = dm.getService<ITcpService>(tcpClientId);
            REQUIRE(svc);
            queue->pushEvent<StopServiceEvent>(tcpClientId, (*svc).first->getClientId(), true);
            co_return {};
        });

        start = std::chrono::steady_clock::now();
        while(evtGate.load(std::memory_order_acquire) != 2) {
            std::this_thread::sleep_for(500us);
            auto now = std::chrono::steady_clock::now();
            REQUIRE(now - start < 1s);
        }

        evtGate.store(0, std::memory_order_release);

        std::this_thread::sleep_for(50ms);

        queue->pushEvent<RunFunctionEventAsync>(0, [&]() -> AsyncGenerator<IchorBehaviour> {
            auto &dm = GetThreadLocalManager();
            auto svcs = dm.getAllServicesOfType<IConnectionService>();
            REQUIRE(svcs.empty());
            queue->pushEvent<QuitEvent>(0);
            co_return {};
        });

        t.join();
    }
}
