// HttpHostService over real sockets, with a raw socket as client: what only shows up in the TCP stack, such as closing gracefully.
#include "Common.h"
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/services/logging/LoggerFactory.h>
#include <ichor/services/logging/NullLogger.h>
#include <ichor/services/network/http/HttpHostService.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>

#ifdef TEST_URING
#include <ichor/services/network/tcp/IOUringTcpHostService.h>
#include <ichor/services/timer/IOUringTimerFactoryFactory.h>
#include <ichor/event_queues/IOUringQueue.h>
#include <ichor/stl/LinuxUtils.h>

#define QIMPL IOUringQueue
#define HOSTIMPL IOUringTcpHostService
#define TFFIMPL IOUringTimerFactoryFactory
constexpr uint16_t PORT = 8014;
#else
#include <ichor/services/network/tcp/TcpHostService.h>
#include <ichor/services/timer/TimerFactoryFactory.h>
#include <ichor/event_queues/PriorityQueue.h>

#define QIMPL PriorityQueue
#define HOSTIMPL TcpHostService
#define TFFIMPL TimerFactoryFactory
constexpr uint16_t PORT = 8013;
#endif

using namespace std::string_literals;

namespace {
    std::vector<uint8_t> toBody(std::string_view str) {
        return {str.begin(), str.end()};
    }

    struct RoutesService final : public AdvancedService<RoutesService> {
        RoutesService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
            reg.registerDependency<IHttpHostService>(this, DependencyFlags::REQUIRED);
        }

        void addDependencyInstance(Ichor::ScopedServiceProxy<IHttpHostService*> host, IService &) {
            _registrations.emplace_back(host->addRoute(HttpMethod::get, "/r", [](HttpRequest &) -> Task<HttpResponse> {
                co_return HttpResponse{.status = HttpStatus::ok, .contentType = "text/plain", .body = toBody("hello"), .headers = {}};
            }));
            _registrations.emplace_back(host->addStreamingRoute(HttpMethod::get, "/stream", [](HttpRequest &, IHttpResponseWriter &writer) -> Task<void> {
                co_await writer.write(toBody("a"));
                co_await writer.write(toBody("bc"));
            }));
            _registrations.emplace_back(host->addStreamingRoute(HttpMethod::get, "/echo", [this](HttpRequest &, IHttpResponseWriter &writer) -> Task<void> {
                HttpResponse response{.status = HttpStatus::switching_protocols, .contentType = {}, .body = {}, .headers = {{"Upgrade", "echo"}}};
                co_await writer.start(std::move(response));
                auto connection = writer.upgrade();
                if(connection) {
                    startEcho(*connection);
                }
            }));
        }

        void removeDependencyInstance(Ichor::ScopedServiceProxy<IHttpHostService*>, IService &) {
            _registrations.clear();
        }

        // Not in the route's coroutine lambda: lambdas in there crash gcc 12
        void startEcho(std::shared_ptr<IHttpUpgradedConnection> connection) {
            _echo = std::move(connection);
            _echo->setReceiveHandler([this](std::span<uint8_t const> data) {
                std::vector<uint8_t> msg{data.begin(), data.end()};
                GetThreadLocalEventQueue().pushEvent<RunFunctionEventAsync>(getServiceId(), [this, msg]() -> AsyncGenerator<IchorBehaviour> {
                    co_await echo(msg);
                    co_return {};
                });
            });
        }

        Task<void> echo(std::vector<uint8_t> msg) {
            co_await _echo->sendAsync(std::move(msg));
        }

        Task<void> stop() final {
            _registrations.clear();
            _echo.reset();
            co_return;
        }

        std::vector<HttpRouteRegistration> _registrations;
        std::shared_ptr<IHttpUpgradedConnection> _echo;
    };

    int connectToServer() {
        auto const start = std::chrono::steady_clock::now();
        while(std::chrono::steady_clock::now() - start < 2s) {
            int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
            REQUIRE(fd >= 0);
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_port = htons(PORT);
            REQUIRE(inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) == 1);
            if(::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0) {
                return fd;
            }
            ::close(fd);
            std::this_thread::sleep_for(10ms);
        }
        FAIL("couldn't connect");
        return -1;
    }

    void sendAll(int fd, std::string_view data) {
        while(!data.empty()) {
            auto const sent = ::send(fd, data.data(), data.size(), MSG_NOSIGNAL);
            REQUIRE(sent > 0);
            data.remove_prefix(static_cast<size_t>(sent));
        }
    }

    struct Received {
        std::string data;
        /// The server closed its sending side, rather than resetting the connection
        bool eof{};
    };

    /// Receives until until() is satisfied, the server closes or a timeout
    template <typename Until>
    Received receive(int fd, Until &&until, std::chrono::milliseconds timeout = 2'000ms) {
        Received ret;
        auto const start = std::chrono::steady_clock::now();
        while(!until(ret.data) && std::chrono::steady_clock::now() - start < timeout) {
            pollfd pfd{.fd = fd, .events = POLLIN, .revents = 0};
            if(::poll(&pfd, 1, 10) <= 0) {
                continue;
            }
            std::array<char, 4096> buf{};
            auto const received = ::recv(fd, buf.data(), buf.size(), 0);
            if(received == 0) {
                ret.eof = true;
                break;
            }
            if(received < 0) {
                break;
            }
            ret.data.append(buf.data(), static_cast<size_t>(received));
        }
        return ret;
    }

    Received receiveUntilClosed(int fd, std::chrono::milliseconds timeout = 2'000ms) {
        return receive(fd, [](std::string const &) { return false; }, timeout);
    }
}

#ifdef TEST_URING
TEST_CASE("HttpHostSocketTests_uring") {
    auto version = Ichor::v1::kernelVersion();
    REQUIRE(version);
    if(version < v1::Version{.major=5, .minor=18, .patch=0}) {
        return;
    }
    auto queue = std::make_unique<QIMPL>(500, 100'000'000);
#else
TEST_CASE("HttpHostSocketTests") {
    auto queue = std::make_unique<QIMPL>(500, true);
#endif
    Ichor::v1::Detail::_local_dm = nullptr;

    std::thread t([&]() {
#ifdef TEST_URING
        REQUIRE(queue->createEventLoop());
#endif
        auto &dm = queue->getManager();
        Ichor::v1::Detail::_local_dm = &dm;
        uint64_t priorityToEnsureHostStartingFirst = 51;
        dm.createServiceManager<CoutFrameworkLogger, IFrameworkLogger>({}, priorityToEnsureHostStartingFirst);
        dm.createServiceManager<LoggerFactory<NullLogger>, ILoggerFactory>({}, priorityToEnsureHostStartingFirst);
        dm.createServiceManager<TFFIMPL>(Properties{}, priorityToEnsureHostStartingFirst);
        dm.createServiceManager<HOSTIMPL, IHostService>(Properties{{"Address", Ichor::v1::make_any<std::string>("127.0.0.1"s)}, {"Port", Ichor::v1::make_any<uint16_t>(PORT)}}, priorityToEnsureHostStartingFirst);
        dm.createServiceManager<HttpHostService, IHttpHostService>(Properties{{"Address", Ichor::v1::make_any<std::string>("127.0.0.1"s)}, {"Port", Ichor::v1::make_any<uint16_t>(PORT)},
                                                                              {"IdleTimeoutMs", Ichor::v1::make_any<uint64_t>(300ULL)}, {"RequestTimeoutMs", Ichor::v1::make_any<uint64_t>(300ULL)},
                                                                              {"LingerTimeoutMs", Ichor::v1::make_any<uint64_t>(2'000ULL)}});
        dm.createServiceManager<RoutesService>();
        queue->start(CaptureSigInt);
    });

    // So that the stop below happens, also when a REQUIRE fails
    struct Quit {
        ~Quit() {
            q.pushEvent<QuitEvent>(ServiceIdType{0});
            thread.join();
        }
        QIMPL &q;
        std::thread &thread;
    } quit{.q = *queue, .thread = t};

    SECTION("closing after a response doesn't reset a client that is still sending") {
        int fd = connectToServer();
        sendAll(fd, "GET /r HTTP/1.1\r\nHost: example.org\r\nConnection: close\r\n\r\n");
        // The client keeps sending, e.g. a body the server didn't want. A server that closes the connection with that unread, or arriving after it
        // closed, resets it, and the client can lose the response (RFC 9112 9.6). Closing gracefully, the server still reads until the client closes.
        std::string const filler(1024, 'x');
        std::string data;
        bool eof{};
        bool reset{};
        auto const start = std::chrono::steady_clock::now();
        tl::optional<std::chrono::steady_clock::time_point> eofAt;
        while(!reset && std::chrono::steady_clock::now() - start < 2s && (!eofAt || std::chrono::steady_clock::now() - *eofAt < 100ms)) {
            if(::send(fd, filler.data(), filler.size(), MSG_NOSIGNAL | MSG_DONTWAIT) < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                reset = true;
                break;
            }
            pollfd pfd{.fd = fd, .events = POLLIN, .revents = 0};
            if(!eof && ::poll(&pfd, 1, 1) > 0) {
                std::array<char, 4096> buf{};
                auto const received = ::recv(fd, buf.data(), buf.size(), 0);
                if(received == 0) {
                    eof = true;
                    eofAt = std::chrono::steady_clock::now();
                } else if(received < 0) {
                    reset = errno == ECONNRESET;
                } else {
                    data.append(buf.data(), static_cast<size_t>(received));
                }
            }
            std::this_thread::sleep_for(1ms);
        }
        ::close(fd);
        REQUIRE(data.starts_with("HTTP/1.1 200 OK\r\n"));
        REQUIRE(data.ends_with("\r\n\r\nhello"));
        REQUIRE(eof);
        REQUIRE(!reset);
    }

    SECTION("streaming") {
        int fd = connectToServer();
        sendAll(fd, "GET /stream HTTP/1.1\r\nHost: example.org\r\n\r\n");
        auto received = receive(fd, [](std::string const &data) { return data.ends_with("0\r\n\r\n"); });
        REQUIRE(received.data.starts_with("HTTP/1.1 200 OK\r\n"));
        REQUIRE(received.data.find("Transfer-Encoding: chunked\r\n") != std::string::npos);
        REQUIRE(received.data.ends_with("\r\n\r\n1\r\na\r\n2\r\nbc\r\n0\r\n\r\n"));
        // the connection can be used again
        sendAll(fd, "GET /r HTTP/1.1\r\nHost: example.org\r\nConnection: close\r\n\r\n");
        received = receiveUntilClosed(fd);
        ::close(fd);
        REQUIRE(received.data.ends_with("hello"));
        REQUIRE(received.eof);
    }

    SECTION("upgrade") {
        int fd = connectToServer();
        sendAll(fd, "GET /echo HTTP/1.1\r\nHost: example.org\r\nUpgrade: echo\r\nConnection: Upgrade\r\n\r\nfirst");
        auto received = receive(fd, [](std::string const &data) { return data.ends_with("first"); });
        REQUIRE(received.data.starts_with("HTTP/1.1 101 Switching Protocols\r\n"));
        REQUIRE(received.data.ends_with("\r\n\r\nfirst"));
        sendAll(fd, "GET / HTTP/1.1\r\n\r\n");
        received = receive(fd, [](std::string const &data) { return data == "GET / HTTP/1.1\r\n\r\n"; });
        ::close(fd);
        REQUIRE(received.data == "GET / HTTP/1.1\r\n\r\n");
    }

    SECTION("a request that takes too long gets 408") {
        int fd = connectToServer();
        sendAll(fd, "GET /r HTTP/1.1\r\nHost: exa");
        auto received = receiveUntilClosed(fd);
        ::close(fd);
        REQUIRE(received.data.starts_with("HTTP/1.1 408 Request Timeout\r\n"));
        REQUIRE(received.data.find("Connection: close\r\n") != std::string::npos);
        REQUIRE(received.eof);
    }

    SECTION("an idle connection is closed") {
        int fd = connectToServer();
        auto received = receiveUntilClosed(fd);
        ::close(fd);
        REQUIRE(received.data.empty());
        REQUIRE(received.eof);
    }
}
