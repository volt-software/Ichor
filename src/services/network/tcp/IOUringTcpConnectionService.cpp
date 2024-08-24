#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32) && !defined(__APPLE__)) || defined(__CYGWIN__)

#include <ichor/DependencyManager.h>
#include <ichor/event_queues/IIOUringQueue.h>
#include <ichor/services/network/tcp/IOUringTcpConnectionService.h>
#include <ichor/services/network/NetworkEvents.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/ScopeGuard.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <ichor/ichor_liburing.h>

uint64_t Ichor::IOUringTcpConnectionService::tcpConnId{};

Ichor::IOUringTcpConnectionService::IOUringTcpConnectionService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)), _socket(-1), _id(tcpConnId++) {
    reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
    reg.registerDependency<IIOUringQueue>(this, DependencyFlags::REQUIRED);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::IOUringTcpConnectionService::start() {
    if(auto propIt = getProperties().find("TimeoutSendUs"); propIt != getProperties().end()) {
        _sendTimeout = Ichor::any_cast<int64_t>(propIt->second);
    }
    if(auto propIt = getProperties().find("TimeoutRecvUs"); propIt != getProperties().end()) {
        _recvTimeout = Ichor::any_cast<int64_t>(propIt->second);
    }
    if(auto propIt = getProperties().find("RecvFunction"); propIt != getProperties().end()) {
        _recvHandler = std::move(Ichor::any_cast<std::function<void(std::span<uint8_t>)>&>(propIt->second));
    } else {
        ICHOR_LOG_ERROR(_logger, "No receive function handler provided");
        co_return tl::unexpected(StartError::FAILED);
    }

    if(auto propIt = getProperties().find("Socket"); propIt != getProperties().end()) {
        _socket = Ichor::any_cast<int>(propIt->second);

        int setting = 1;
        ::setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, &setting, sizeof(setting));

        timeval timeout{};
        timeout.tv_usec = _recvTimeout;
        setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        timeout.tv_usec = _sendTimeout;
        setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        ICHOR_LOG_TRACE(_logger, "[{}] Starting TCP connection for existing socket", _id);
    } else {
        auto addrIt = getProperties().find("Address");
        auto portIt = getProperties().find("Port");

        if(addrIt == getProperties().end()) {
            ICHOR_LOG_ERROR(_logger, "[{}] Missing address", _id);
            co_return tl::unexpected(StartError::FAILED);
        }
        if(portIt == getProperties().end()) {
            ICHOR_LOG_ERROR(_logger, "[{}] Missing port", _id);
            co_return tl::unexpected(StartError::FAILED);
        }

        AsyncManualResetEvent evt;

        auto *sqe = _q->getSqe();
        int res{};
        io_uring_sqe_set_data(sqe, new UringResponseEvent{_q->getNextEventIdFromIchor(), getServiceId(), INTERNAL_EVENT_PRIORITY, [&evt, &res](int32_t _res) {
            INTERNAL_IO_DEBUG("socket res: {} {}", _res, _res < 0 ? strerror(-_res) : "");
            res = _res;
            evt.set();
        }});
        io_uring_prep_socket(sqe, AF_INET, SOCK_STREAM, O_CLOEXEC, 0);
        co_await evt;

        if(res < 0) {
            ICHOR_LOG_ERROR(_logger, "Couldn't open a socket to {}:{}: {}", Ichor::any_cast<std::string&>(addrIt->second), Ichor::any_cast<uint16_t>(portIt->second), mapErrnoToError(-res));
            co_return tl::unexpected(StartError::FAILED);
        }

        _socket = res;

        int setting = 1;
        ::setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, &setting, sizeof(setting));

        timeval timeout{};
        timeout.tv_usec = _recvTimeout;
        setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        timeout.tv_usec = _sendTimeout;
        setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(Ichor::any_cast<uint16_t>(portIt->second));

        int ret = inet_pton(AF_INET, Ichor::any_cast<std::string&>(addrIt->second).c_str(), &address.sin_addr);
        if(ret == 0)
        {
            throw std::runtime_error("inet_pton invalid address for given address family (has to be ipv4-valid address)");
        }

        sqe = _q->getSqe();
        res = 0;
        evt.reset();
        io_uring_sqe_set_data(sqe, new UringResponseEvent{_q->getNextEventIdFromIchor(), getServiceId(), INTERNAL_EVENT_PRIORITY, [&evt, &res](int32_t _res) {
            INTERNAL_IO_DEBUG("connect res: {} {}", _res, _res < 0 ? strerror(-_res) : "");
            res = _res;
            evt.set();
        }});
        io_uring_prep_connect(sqe, _socket, (struct sockaddr *)&address, sizeof(address));
        co_await evt;

        ICHOR_LOG_TRACE(_logger, "[{}] Starting TCP connection for {}:{}", _id, Ichor::any_cast<std::string&>(addrIt->second), Ichor::any_cast<uint16_t>(portIt->second));
    }

    if(_q->getKernelVersion() >= Version{6, 0, 0}) {
        auto *sqe = _q->getSqe();
        io_uring_sqe_set_data(sqe, new UringResponseEvent{_q->getNextEventIdFromIchor(), getServiceId(), INTERNAL_EVENT_PRIORITY, [this](int32_t _res) {
            INTERNAL_IO_DEBUG("recv multishot res: {} {}", _res, _res < 0 ? strerror(-_res) : "");

            if (_quit) {
                return;
            }

            if (_res >= 0) {
                _recvHandler(std::span<uint8_t>{_recvBuf.begin(), _recvBuf.end()});
                _recvBuf.clear();
            }
        }});
        io_uring_sqe_set_flags(sqe, IOSQE_BUFFER_SELECT);
        io_uring_prep_recv_multishot(sqe, _socket, nullptr, 0, 0);
    } else {
        if(auto propIt = getProperties().find("RecvBufferSize"); propIt != getProperties().end()) {
            _recvBuf.reserve(Ichor::any_cast<uint64_t>(propIt->second));
        } else {
            _recvBuf.reserve(2048);
        }

        auto *sqe = _q->getSqe();
        io_uring_sqe_set_data(sqe, new UringResponseEvent{_q->getNextEventIdFromIchor(), getServiceId(), INTERNAL_EVENT_PRIORITY, createRecvHandler()});
        io_uring_prep_recv(sqe, _socket, _recvBuf.data(), _recvBuf.size(), 0);
    }

    co_return {};
}

Ichor::Task<void> Ichor::IOUringTcpConnectionService::stop() {
    _quit = true;

    if(_socket >= 0) {
        AsyncManualResetEvent evt;
        auto *sqe = _q->getSqe();
        int res{};
        io_uring_sqe_set_data(sqe, new UringResponseEvent{_q->getNextEventIdFromIchor(), getServiceId(), INTERNAL_EVENT_PRIORITY, [&evt, &res](int32_t _res) {
            INTERNAL_IO_DEBUG("shutdown res: {} {}", _res, _res < 0 ? strerror(-_res) : "");
            res = _res;
            evt.set();
        }});
        io_uring_prep_shutdown(sqe, _socket, SHUT_RDWR);
        co_await evt;

        if(res < 0) {
            ICHOR_LOG_ERROR(_logger, "Couldn't shutdown socket: {}", mapErrnoToError(-res));
        }

        sqe = _q->getSqe();
        res = 0;
        evt.reset();
        io_uring_sqe_set_data(sqe, new UringResponseEvent{_q->getNextEventIdFromIchor(), getServiceId(), INTERNAL_EVENT_PRIORITY, [&evt, &res](int32_t _res) {
            INTERNAL_IO_DEBUG("close res: {} {}", _res, _res < 0 ? strerror(-_res) : "");
            res = _res;
            evt.set();
        }});
        io_uring_prep_close(sqe, _socket);
        co_await evt;

        if(res < 0) {
            ICHOR_LOG_ERROR(_logger, "Couldn't close socket: {}", mapErrnoToError(-res));
        }

        _socket = 0;
    }

    co_return;
}

void Ichor::IOUringTcpConnectionService::addDependencyInstance(ILogger &logger, IService &) noexcept {
    _logger = &logger;
}

void Ichor::IOUringTcpConnectionService::removeDependencyInstance(ILogger &, IService&) noexcept {
    _logger = nullptr;
}

void Ichor::IOUringTcpConnectionService::addDependencyInstance(IIOUringQueue &q, IService&) noexcept {
    _q = &q;
}

void Ichor::IOUringTcpConnectionService::removeDependencyInstance(IIOUringQueue&, IService&) noexcept {
    _q = nullptr;
}

std::function<void(int32_t)> Ichor::IOUringTcpConnectionService::createRecvHandler() noexcept {
    return [this](int32_t _res) {
        INTERNAL_IO_DEBUG("recv res: {} {}", _res, _res < 0 ? strerror(-_res) : "");

        if (_quit || _res <= 0) {
            return;
        }

        _recvHandler(std::span<uint8_t>{_recvBuf.begin(), _recvBuf.end()});
        _recvBuf.clear();

        auto *sqe = _q->getSqe();
        io_uring_sqe_set_data(sqe, new UringResponseEvent{_q->getNextEventIdFromIchor(), getServiceId(), INTERNAL_EVENT_PRIORITY, createRecvHandler()});
        io_uring_prep_recv(sqe, _socket, _recvBuf.data(), _recvBuf.size(), 0);
    };
}

tl::expected<uint64_t, Ichor::SendErrorReason> Ichor::IOUringTcpConnectionService::sendAsync(std::vector<uint8_t> &&msg) {
    auto id = ++_msgIdCounter;
    size_t sent_bytes = 0;

    if(_quit) {
        ICHOR_LOG_TRACE(_logger, "[{}] quitting, no send", _id);
        return tl::unexpected(SendErrorReason::QUITTING);
    }

    while(sent_bytes < msg.size()) {
        auto ret = ::send(_socket, msg.data() + sent_bytes, msg.size() - sent_bytes, 0);

        if(_quit) {
            ICHOR_LOG_TRACE(_logger, "[{}] quitting mid-send", _id);
            return tl::unexpected(SendErrorReason::QUITTING);
        }

        if(ret < 0) {
            GetThreadLocalEventQueue().pushEvent<FailedSendMessageEvent>(getServiceId(), std::move(msg), id);
            break;
        }

        sent_bytes += static_cast<uint64_t>(ret);
    }

    return id;
}

void Ichor::IOUringTcpConnectionService::setPriority(uint64_t priority) {

}

uint64_t Ichor::IOUringTcpConnectionService::getPriority() {
    return 0;
}

#endif
