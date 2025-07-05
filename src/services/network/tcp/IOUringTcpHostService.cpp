#include <ichor/DependencyManager.h>
#include <ichor/services/network/tcp/IOUringTcpHostService.h>
#include <ichor/services/network/tcp/IOUringTcpConnectionService.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/ScopeGuard.h>
#include <ichor/Filter.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <netdb.h>
#include <ichor/ichor_liburing.h>

Ichor::v1::IOUringTcpHostService::IOUringTcpHostService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)), _socket(-1), _bindFd(), _priority(INTERNAL_EVENT_PRIORITY), _quit() {
    reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
    reg.registerDependency<IIOUringQueue>(this, DependencyFlags::REQUIRED);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::v1::IOUringTcpHostService::start() {
    if(auto propIt = getProperties().find("Priority"); propIt != getProperties().end()) {
        _priority = Ichor::v1::any_cast<uint64_t>(propIt->second);
    }
    if(auto propIt = getProperties().find("ListenBacklogSize"); propIt != getProperties().end()) {
        _listenBacklogSize = Ichor::v1::any_cast<uint16_t>(propIt->second);
    }
	if(auto propIt = getProperties().find("BufferEntries"); propIt != getProperties().end()) {
		_bufferEntries = Ichor::v1::any_cast<uint32_t>(propIt->second);
	}
	if(auto propIt = getProperties().find("BufferEntrySize"); propIt != getProperties().end()) {
		_bufferEntrySize = Ichor::v1::any_cast<uint32_t>(propIt->second);
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

        if(res < 0) {
            ICHOR_LOG_ERROR(_logger, "Couldn't open a socket: {}", mapErrnoToError(-res));
            co_return tl::unexpected(StartError::FAILED);
        }
        _socket = res;
    } else {
        _socket = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    }

    if(_q->getKernelVersion() >= Version{6, 7, 0}) {
        int resReuse{};
        int resNodelay{};
        AsyncManualResetEvent evtSockopt;
        int setting = 1;
        auto *sqe = _q->getSqeWithData(this, [&resReuse](io_uring_cqe *cqe) {
            INTERNAL_IO_DEBUG("setsockopt SO_REUSEADDR res: {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
            resReuse = cqe->res;
        });
        sqe->flags |= IOSQE_IO_HARDLINK;
        io_uring_prep_cmd_sock(sqe, SOCKET_URING_OP_SETSOCKOPT, _socket, SOL_SOCKET, SO_REUSEADDR, &setting, sizeof(setting));
        sqe = _q->getSqeWithData(this, [&evtSockopt, &resNodelay](io_uring_cqe *cqe) {
            INTERNAL_IO_DEBUG("setsockopt TCP_NODELAY res: {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
            resNodelay = cqe->res;
            evtSockopt.set();
        });
        sqe->flags |= IOSQE_IO_HARDLINK;
        io_uring_prep_cmd_sock(sqe, SOCKET_URING_OP_SETSOCKOPT, _socket, IPPROTO_TCP, TCP_NODELAY, &setting, sizeof(setting));

        co_await evtSockopt;

        if(resReuse < 0) {
            ICHOR_LOG_ERROR(_logger, "failed to set SO_REUSEADDR");
        }
        if(resNodelay < 0) {
            ICHOR_LOG_ERROR(_logger, "failed to set TCP_NODELAY");
        }
    } else {
        int setting = 1;
        if(::setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &setting, sizeof(setting)) != 0) {
            ICHOR_LOG_ERROR(_logger, "failed to set SO_REUSEADDR");
        }
        if(::setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, &setting, sizeof(setting)) != 0) {
            ICHOR_LOG_ERROR(_logger, "failed to set TCP_NODELAY");
        }
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;

    auto const addressProp = getProperties().find("Address");

    if(addressProp != cend(getProperties())) {
        auto hostname = Ichor::v1::any_cast<std::string>(addressProp->second);
        if(::inet_aton(hostname.c_str(), &address.sin_addr) != 0) {
            auto *hp = ::gethostbyname(hostname.c_str());
            if (hp == nullptr) {
                close(_socket);
                _socket = -1;
                ICHOR_LOG_ERROR(_logger, "Couldn't get host by name for hostname {}: {}", hostname, mapErrnoToError(errno));
                co_return tl::unexpected(StartError::FAILED);
            }

            memcpy(&address.sin_addr, hp->h_addr, sizeof(address.sin_addr));
        }
    } else {
        address.sin_addr.s_addr = INADDR_ANY;
    }
    address.sin_port = ::htons(Ichor::v1::any_cast<uint16_t>((getProperties())["Port"]));

    // io_uring_prep_bind is not yet available in the kernel.
    _bindFd = ::bind(_socket, (sockaddr *)&address, sizeof(address));

    if(_bindFd == -1) {
        close(_socket);
        _socket = -1;
        ICHOR_LOG_ERROR(_logger, "Couldn't bind socket with address {} port {}: {}", addressProp == cend(getProperties()) ? std::string{"ANY"} : Ichor::v1::any_cast<std::string>(addressProp->second), Ichor::v1::any_cast<uint16_t>((getProperties())["Port"]), mapErrnoToError(errno));
        co_return tl::unexpected(StartError::FAILED);
    }

    // io_uring_prep_listen is not yet available in the kernel.
    if(::listen(_socket, _listenBacklogSize) != 0) {
        close(_socket);
        _socket = -1;
        co_return tl::unexpected(StartError::FAILED);
    }

    if(_q->getKernelVersion() >= Version{5, 19, 0}) {
        auto *sqe = _q->getSqeWithData(this, createAcceptHandler());
        io_uring_prep_multishot_accept(sqe, _socket, nullptr, nullptr, SOCK_CLOEXEC);
    } else {
        auto *sqe = _q->getSqeWithData(this, createAcceptHandler());
        io_uring_prep_accept(sqe, _socket, nullptr, nullptr, SOCK_CLOEXEC);
    }

    co_return {};
}

Ichor::Task<void> Ichor::v1::IOUringTcpHostService::stop() {
    _quit = true;
    INTERNAL_IO_DEBUG("quit");

    if(_socket >= 0) {
        if(_q->getKernelVersion() >= Version{5, 19, 0}) {
            int shutdownRes{};
            int closeRes{};
            AsyncManualResetEvent evt;
            if(_q->sqeSpaceLeft() < 2) {
                _q->forceSubmit();
            }
            auto *sqe = _q->getSqeWithData(this, [&shutdownRes](io_uring_cqe *cqe) {
                INTERNAL_IO_DEBUG("shutdown res: {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
                shutdownRes = cqe->res;
            });
            sqe->flags |= IOSQE_IO_HARDLINK;
            io_uring_prep_shutdown(sqe, _socket, SHUT_RDWR);

            sqe = _q->getSqeWithData(this, [&evt, &closeRes](io_uring_cqe *cqe) {
                INTERNAL_IO_DEBUG("close res: {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
                closeRes = cqe->res;
                evt.set();
            });
            io_uring_prep_close(sqe, _socket);

            co_await evt;

            if(shutdownRes < 0) {
                ICHOR_LOG_ERROR(_logger, "Couldn't shutdown socket: {}", mapErrnoToError(-shutdownRes));
            }
            if(closeRes < 0) {
                ICHOR_LOG_ERROR(_logger, "Couldn't close socket: {}", mapErrnoToError(-closeRes));
            }

            co_await _quitEvt;
        } else {
            int res = shutdown(_socket, SHUT_RDWR);

            if(res < 0) {
                ICHOR_LOG_ERROR(_logger, "Couldn't shutdown socket: {}", mapErrnoToError(errno));
            }

            res = close(_socket);

            if(res < 0) {
                ICHOR_LOG_ERROR(_logger, "Couldn't close socket: {}", mapErrnoToError(errno));
            }

            co_await _quitEvt;
        }

        _socket = 0;
    }

    co_return;
}

void Ichor::v1::IOUringTcpHostService::addDependencyInstance(ILogger &logger, IService &) noexcept {
    _logger = &logger;
}

void Ichor::v1::IOUringTcpHostService::removeDependencyInstance(ILogger &, IService&) noexcept {
    _logger = nullptr;
}

void Ichor::v1::IOUringTcpHostService::addDependencyInstance(IIOUringQueue &q, IService &) noexcept {
    _q = &q;
}

void Ichor::v1::IOUringTcpHostService::removeDependencyInstance(IIOUringQueue &, IService&) noexcept {
    _q = nullptr;
}

void Ichor::v1::IOUringTcpHostService::setPriority(uint64_t priority) {
    _priority = priority;
}

uint64_t Ichor::v1::IOUringTcpHostService::getPriority() {
    return _priority;
}

std::function<void(io_uring_cqe *)> Ichor::v1::IOUringTcpHostService::createAcceptHandler() noexcept {
    return [this](io_uring_cqe *cqe) {
        INTERNAL_IO_DEBUG("accept res: {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");

        if (_quit) {
            INTERNAL_IO_DEBUG("quit");
            _quitEvt.set();
            return;
        }

        if (cqe->res <= 0) {
            ICHOR_LOG_ERROR(_logger, "accept returned an error {}:{}", cqe->res, strerror(-cqe->res));
            GetThreadLocalEventQueue().pushEvent<StopServiceEvent>(getServiceId(), getServiceId(), true);
            _quitEvt.set();
            return;
        }

        if(_logger->getLogLevel() == LogLevel::LOG_TRACE) {
            sockaddr_in client_addr{};
            socklen_t addr_size = sizeof(client_addr);
            if(getpeername(cqe->res, (struct sockaddr *)&client_addr, &addr_size) != 0) {
                ICHOR_LOG_ERROR(_logger, "new connection, but could not get peername");
            } else {
                auto *ip = ::inet_ntoa(client_addr.sin_addr);
                ICHOR_LOG_TRACE(_logger, "new connection from {}:{}", ip, ::ntohs(client_addr.sin_port));
            }
        }

        Properties props{};
        props.reserve(7);
        props.emplace("Priority", Ichor::v1::make_any<uint64_t>(_priority));
        props.emplace("Socket", Ichor::v1::make_any<int>(cqe->res));
        props.emplace("TimeoutSendUs", Ichor::v1::make_any<int64_t>(_sendTimeout));
        props.emplace("TimeoutRecvUs", Ichor::v1::make_any<int64_t>(_recvTimeout));
        props.emplace("TcpHostService", Ichor::v1::make_any<ServiceIdType>(getServiceId()));
		if(_bufferEntries) {
			props.emplace("BufferEntries", Ichor::v1::make_any<uint32_t>(*_bufferEntries));
		}
		if(_bufferEntrySize) {
			props.emplace("BufferEntrySize", Ichor::v1::make_any<uint32_t>(*_bufferEntrySize));
		}
        _connections.emplace_back(GetThreadLocalManager().template createServiceManager<IOUringTcpConnectionService<IHostConnectionService>, IConnectionService, IHostConnectionService>(std::move(props))->getServiceId());

        if(_q->getKernelVersion() < Version{5, 19, 0}) {
            auto *sqe = _q->getSqeWithData(this, createAcceptHandler());
            io_uring_prep_accept(sqe, _socket, nullptr, nullptr, SOCK_CLOEXEC);
        } else if((cqe->flags & IORING_CQE_F_MORE) != IORING_CQE_F_MORE) {
            auto *sqe = _q->getSqeWithData(this, createAcceptHandler());
            io_uring_prep_multishot_accept(sqe, _socket, nullptr, nullptr, SOCK_CLOEXEC);
        }
    };
}
