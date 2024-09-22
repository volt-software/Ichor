#include <ichor/DependencyManager.h>
#include <ichor/event_queues/IIOUringQueue.h>
#include <ichor/services/network/tcp/IOUringTcpConnectionService.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/ScopeGuard.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <thread>
#include <ichor/ichor_liburing.h>

uint64_t Ichor::IOUringTcpConnectionService::tcpConnId{};

Ichor::IOUringTcpConnectionService::IOUringTcpConnectionService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)), _socket(-1), _id(tcpConnId++) {
    reg.registerDependency<ILogger>(this, DependencyFlags::NONE);
    reg.registerDependency<IIOUringQueue>(this, DependencyFlags::REQUIRED);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::IOUringTcpConnectionService::start() {
    if(_q->getKernelVersion() < Version{5, 5, 0}) {
        fmt::println("Kernel version too old to use IOUringTcpConnectionService. Requires >= 5.5.0");
    }

    if(auto propIt = getProperties().find("TimeoutSendUs"); propIt != getProperties().end()) {
        _sendTimeout = Ichor::any_cast<int64_t>(propIt->second);
    }
    if(auto propIt = getProperties().find("TimeoutRecvUs"); propIt != getProperties().end()) {
        _recvTimeout = Ichor::any_cast<int64_t>(propIt->second);
    }
    if(auto propIt = getProperties().find("BufferEntries"); propIt != getProperties().end()) {
        _bufferEntries = Ichor::any_cast<uint32_t>(propIt->second);
    }
    if(auto propIt = getProperties().find("BufferEntrySize"); propIt != getProperties().end()) {
        _bufferEntrySize = Ichor::any_cast<uint32_t>(propIt->second);
    }
    auto socketPropIt = getProperties().find("Socket");
    auto addrIt = getProperties().find("Address");
    auto portIt = getProperties().find("Port");

    if(socketPropIt != getProperties().end()) {
        _socket = Ichor::any_cast<int>(socketPropIt->second);

        ICHOR_LOG_TRACE(_logger, "[{}] Starting TCP connection for existing socket", _id);
    } else {

        if(addrIt == getProperties().end()) {
            ICHOR_LOG_ERROR(_logger, "[{}] Missing address", _id);
            co_return tl::unexpected(StartError::FAILED);
        }
        if(portIt == getProperties().end()) {
            ICHOR_LOG_ERROR(_logger, "[{}] Missing port", _id);
            co_return tl::unexpected(StartError::FAILED);
        }


        if(_q->getKernelVersion() >= Version{5, 19, 0}) {
            AsyncManualResetEvent evt;

            int res{};
            auto *sqe = _q->getSqeWithData(this, [&evt, &res](io_uring_cqe *cqe) {
                INTERNAL_IO_DEBUG("socket res: {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
                res = cqe->res;
                evt.set();
            });
            io_uring_prep_socket(sqe, AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0, 0);
            co_await evt;

            if (res < 0) {
                ICHOR_LOG_ERROR(_logger, "Couldn't open a socket to {}:{}: {}", Ichor::any_cast<std::string &>(addrIt->second),
                                Ichor::any_cast<uint16_t>(portIt->second), mapErrnoToError(-res));
                co_return tl::unexpected(StartError::FAILED);
            }

            _socket = res;
        } else {
            _socket = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        }
    }

    if(_q->getKernelVersion() >= Version{6, 7, 0}) {
        int setting = 1;
        int resNodelay{-1};
        int resRcvtimeo{-1};
        int resSndtimeo{-1};
        timeval timeout{};
        AsyncManualResetEvent evtSockopt;
        if(_q->sqeSpaceLeft() < 3) {
            _q->forceSubmit();
        }
        auto *sqe = _q->getSqeWithData(this, [&resNodelay](io_uring_cqe *cqe) {
            INTERNAL_IO_DEBUG("setsockopt TCP_NODELAY res: {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
            resNodelay = cqe->res;
        });
        sqe->flags |= IOSQE_IO_HARDLINK;
        io_uring_prep_cmd_sock(sqe, SOCKET_URING_OP_SETSOCKOPT, _socket, IPPROTO_TCP, TCP_NODELAY, &setting, sizeof(setting));

        timeout.tv_usec = _recvTimeout;
        sqe = _q->getSqeWithData(this, [&resRcvtimeo](io_uring_cqe *cqe) {
            INTERNAL_IO_DEBUG("setsockopt SO_RCVTIMEO res: {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
            resRcvtimeo = cqe->res;
        });
        sqe->flags |= IOSQE_IO_HARDLINK;
        io_uring_prep_cmd_sock(sqe, SOCKET_URING_OP_SETSOCKOPT, _socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        timeout.tv_usec = _sendTimeout;
        sqe = _q->getSqeWithData(this, [&evtSockopt, &resSndtimeo](io_uring_cqe *cqe) {
            INTERNAL_IO_DEBUG("setsockopt SO_SNDTIMEO res: {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
            resSndtimeo = cqe->res;
            evtSockopt.set();
        });
        io_uring_prep_cmd_sock(sqe, SOCKET_URING_OP_SETSOCKOPT, _socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        co_await evtSockopt;

        if(resNodelay < 0) {
            ICHOR_LOG_ERROR(_logger, "failed to set TCP_NODELAY");
        }
        if(resRcvtimeo < 0) {
            ICHOR_LOG_ERROR(_logger, "failed to set SO_RCVTIMEO");
        }
        if(resSndtimeo < 0) {
            ICHOR_LOG_ERROR(_logger, "failed to set SO_SNDTIMEO");
        }
    } else {
        int setting = 1;
        if(::setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, &setting, sizeof(setting)) != 0) {
            ICHOR_LOG_ERROR(_logger, "failed to set TCP_NODELAY");
        }
        timeval timeout{};
        timeout.tv_usec = _recvTimeout;
        if(::setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
            ICHOR_LOG_ERROR(_logger, "failed to set SO_RCVTIMEO");
        }
        timeout.tv_usec = _sendTimeout;
        if(::setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) {
            ICHOR_LOG_ERROR(_logger, "failed to set SO_SNDTIMEO");
        }
    }

    if(socketPropIt == getProperties().end()) {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(Ichor::any_cast<uint16_t>(portIt->second));

        int ret = inet_pton(AF_INET, Ichor::any_cast<std::string&>(addrIt->second).c_str(), &address.sin_addr);
        if(ret == 0)
        {
            throw std::runtime_error("inet_pton invalid address for given address family (has to be ipv4-valid address)");
        }

        int res = 0;
        AsyncManualResetEvent evt;
        auto *sqe = _q->getSqeWithData(this, [&evt, &res](io_uring_cqe *cqe) {
            INTERNAL_IO_DEBUG("connect res: {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
            res = cqe->res;
            evt.set();
        });
        io_uring_prep_connect(sqe, _socket, (struct sockaddr *)&address, sizeof(address));
        co_await evt;

        ICHOR_LOG_TRACE(_logger, "[{}] Starting TCP connection for {}:{}", _id, Ichor::any_cast<std::string&>(addrIt->second), Ichor::any_cast<uint16_t>(portIt->second));
    }

    bool armedMultishot{};
    if(_q->getKernelVersion() >= Version{6, 0, 0}) {
        auto buffer = _q->createProvidedBuffer(static_cast<unsigned short>(_bufferEntries), _bufferEntrySize);
        if(buffer) {
            _buffer = std::move(*buffer);
            auto *sqe = _q->getSqeWithData(this, createRecvHandler());
            io_uring_prep_recv_multishot(sqe, _socket, nullptr, 0, 0);
            sqe->buf_group = static_cast<__u16>(_buffer->getBufferGroupId());
            sqe->flags |= IOSQE_BUFFER_SELECT;
            armedMultishot = true;
        } else {
            ICHOR_LOG_WARN(_logger, "Couldn't create provided buffers: {}", buffer.error());
        }
    }
    if(!armedMultishot) {
        if(auto propIt = getProperties().find("RecvBufferSize"); propIt != getProperties().end()) {
            _recvBuf.reserve(Ichor::any_cast<size_t>(propIt->second));
            ICHOR_LOG_WARN(_logger, "_recvBuf size {}", _recvBuf.size());
        } else {
            _recvBuf.resize(2048);
            ICHOR_LOG_WARN(_logger, "_recvBuf size2 {}", _recvBuf.size());
        }

        auto *sqe = _q->getSqeWithData(this, createRecvHandler());
        io_uring_prep_recv(sqe, _socket, _recvBuf.data(), _recvBuf.size(), 0);
    }

    co_return {};
}

Ichor::Task<void> Ichor::IOUringTcpConnectionService::stop() {
    _quit = true;
    INTERNAL_IO_DEBUG("quit");

    if(_socket >= 0) {
        AsyncManualResetEvent evt;
        int res{};
        auto *sqe = _q->getSqeWithData(this, [&evt, &res](io_uring_cqe *cqe) {
            INTERNAL_IO_DEBUG("shutdown res: {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
            res = cqe->res;
            evt.set();
        });
        io_uring_prep_shutdown(sqe, _socket, SHUT_RDWR);

        co_await evt;

        if(res < 0) {
            ICHOR_LOG_ERROR(_logger, "Couldn't shutdown socket: {}", mapErrnoToError(-res));
        }

        res = 0;
        evt.reset();
        sqe = _q->getSqeWithData(this, [&evt, &res](io_uring_cqe *cqe) {
            INTERNAL_IO_DEBUG("close res: {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
            res = cqe->res;
            evt.set();
        });
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

std::function<void(io_uring_cqe*)> Ichor::IOUringTcpConnectionService::createRecvHandler() noexcept {
    return [this](io_uring_cqe *cqe) {
        INTERNAL_IO_DEBUG("recv res: {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");

        if(_quit) {
            INTERNAL_IO_DEBUG("quit");
            return;
        }
        // TODO: check for -ENOBUFS and if so, create more provided buffers, swap and re-arm
        if(cqe->res <= 0) {
            ICHOR_LOG_ERROR(_logger, "recv returned an error {}:{}", cqe->res, strerror(-cqe->res));
            GetThreadLocalEventQueue().pushEvent<StopServiceEvent>(getServiceId(), getServiceId(), true);
            return;
        }

        if(_buffer) {
            if((cqe->flags & IORING_CQE_F_BUFFER) != IORING_CQE_F_BUFFER) {
                ICHOR_LOG_ERROR(_logger, "no buffer to cqe, connection probably closed? {}", cqe->res);
                return;
            }

            auto entry = cqe->flags >> IORING_CQE_BUFFER_SHIFT;
            auto entryData = _buffer->readMemory(entry);
            auto data = std::span<uint8_t const>{reinterpret_cast<uint8_t const*>(entryData.data()), std::min(entryData.size(), static_cast<decltype(entryData.size())>(cqe->res))};
//            fmt::println("received {} len, entry {}, {} {}", cqe->res, entry, _bufferEntries, _bufferEntrySize);
            if(_recvHandler) {
                _recvHandler(data);
            } else {
                auto &copy = _queuedMessages.emplace_back();
                copy.assign(data.begin(), data.end());
            }
            _buffer->markEntryAvailableAgain(static_cast<unsigned short>(entry));
        } else {
            if(_recvHandler) {
                _recvHandler(std::span<uint8_t const>{_recvBuf.begin(), _recvBuf.begin() + cqe->res});
            } else {
                _queuedMessages.emplace_back(std::move(_recvBuf));
            }

            auto *sqe = _q->getSqeWithData(this, createRecvHandler());
            io_uring_prep_recv(sqe, _socket, _recvBuf.data(), _recvBuf.size(), 0);
        }
    };
}

Ichor::Task<tl::expected<void, Ichor::IOError>> Ichor::IOUringTcpConnectionService::sendAsync(std::vector<uint8_t> &&msg) {
    size_t sent_bytes = 0;

    if(_quit) {
        ICHOR_LOG_TRACE(_logger, "[{}] quitting, no send", _id);
        co_return tl::unexpected(IOError::SERVICE_QUITTING);
    }

    while(sent_bytes < msg.size()) {
        if(_quit) {
            ICHOR_LOG_TRACE(_logger, "[{}] quitting, no send", _id);
            co_return tl::unexpected(IOError::SERVICE_QUITTING);
        }

        AsyncManualResetEvent evt{};
        int32_t res{};
        auto *sqe = _q->getSqeWithData(this, [&res, &evt](io_uring_cqe *cqe) {
            res = cqe->res;
            evt.set();
        });
        io_uring_prep_send(sqe, _socket, msg.data() + sent_bytes, msg.size() - sent_bytes, 0);
        co_await evt;
        if(res < 0) {
            auto ret = mapErrnoToError(res);
            ICHOR_LOG_ERROR(_logger, "Couldn't send message: {}", ret);
            co_return tl::unexpected(ret);
        }
//        fmt::println("sent {} bytes", res);

        sent_bytes += static_cast<size_t>(res);
    }

    INTERNAL_IO_DEBUG("sending done");
    co_return {};
}

Ichor::Task<tl::expected<void, Ichor::IOError>> Ichor::IOUringTcpConnectionService::sendAsync(std::vector<std::vector<uint8_t>> &&msgs) {
    if(_quit) {
        ICHOR_LOG_TRACE(_logger, "[{}] quitting, no send", _id);
        co_return tl::unexpected(IOError::SERVICE_QUITTING);
    }

    if(_quit) {
        ICHOR_LOG_TRACE(_logger, "[{}] quitting, no send", _id);
        co_return tl::unexpected(IOError::SERVICE_QUITTING);
    }

    AsyncManualResetEvent evt{};
    int32_t res{};
    uint64_t totalBytes{};
    msghdr hdr{};
    if(msgs.size() < 64) {
        iovec vecs[64];
        hdr.msg_iov = vecs;
        hdr.msg_iovlen = msgs.size();
        for(uint64_t i = 0; i < msgs.size(); i++) {
            vecs[i].iov_base = msgs[i].data();
            vecs[i].iov_len = msgs[i].size();
            totalBytes += msgs[i].size();
        }
        auto *sqe = _q->getSqeWithData(this, [&res, &evt](io_uring_cqe *cqe) {
            res = cqe->res;
            evt.set();
        });
        io_uring_prep_sendmsg(sqe, _socket, &hdr, 0);
        co_await evt;
    } else {
        std::vector<iovec> vecs;
        vecs.resize(msgs.size());
        hdr.msg_iov = vecs.data();
        hdr.msg_iovlen = msgs.size();
        for(uint64_t i = 0; i < msgs.size(); i++) {
            vecs[i].iov_base = msgs[i].data();
            vecs[i].iov_len = msgs[i].size();
            totalBytes += msgs[i].size();
        }
        auto *sqe = _q->getSqeWithData(this, [&res, &evt](io_uring_cqe *cqe) {
            res = cqe->res;
            evt.set();
        });
        io_uring_prep_sendmsg(sqe, _socket, &hdr, 0);
        co_await evt;
    }
    if(res < 0) {
        auto ret = mapErrnoToError(res);
        ICHOR_LOG_ERROR(_logger, "Couldn't send message: {} {} {} {}", ret, res, msgs.size(), totalBytes);
        co_return tl::unexpected(ret);
    }
//    fmt::println("sent {} bytes, expected to send {} bytes", res, totalBytes);
    if(static_cast<uint64_t>(res) != totalBytes) {
        std::terminate();
    }

    INTERNAL_IO_DEBUG("sending done");
    co_return {};
}

void Ichor::IOUringTcpConnectionService::setPriority(uint64_t priority) {

}

uint64_t Ichor::IOUringTcpConnectionService::getPriority() {
    return 0;
}

bool Ichor::IOUringTcpConnectionService::isClient() const noexcept {
    return getProperties().find("Socket") == getProperties().end();
}

void Ichor::IOUringTcpConnectionService::setReceiveHandler(std::function<void(std::span<uint8_t const>)> recvHandler) {
    _recvHandler = recvHandler;

    for(auto &msg : _queuedMessages) {
        _recvHandler(msg);
    }
    _queuedMessages.clear();
}
