#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32) && !defined(__APPLE__)) || defined(__CYGWIN__)

#include <ichor/DependencyManager.h>
#include <ichor/services/network/tcp/TcpConnectionService.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/ScopeGuard.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <thread>

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::IConnectionService, Ichor::IHostConnectionService, Ichor::IClientConnectionService>
Ichor::TcpConnectionService<InterfaceT>::TcpConnectionService(DependencyRegister &reg, Properties props) : AdvancedService<TcpConnectionService<InterfaceT>>(std::move(props)), _socket(-1), _attempts(), _priority(INTERNAL_EVENT_PRIORITY), _quit() {
    reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
    reg.registerDependency<ITimerFactory>(this, DependencyFlags::REQUIRED);
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::IConnectionService, Ichor::IHostConnectionService, Ichor::IClientConnectionService>
Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::TcpConnectionService<InterfaceT>::start() {
    if(auto propIt = AdvancedService<TcpConnectionService>::getProperties().find("Priority"); propIt != AdvancedService<TcpConnectionService>::getProperties().end()) {
        _priority = Ichor::any_cast<uint64_t>(propIt->second);
    }
    if(auto propIt = AdvancedService<TcpConnectionService>::getProperties().find("TimeoutSendUs"); propIt != AdvancedService<TcpConnectionService>::getProperties().end()) {
        _sendTimeout = Ichor::any_cast<int64_t>(propIt->second);
    }

    if(AdvancedService<TcpConnectionService>::getProperties().contains("Socket")) {
        if(auto propIt = AdvancedService<TcpConnectionService>::getProperties().find("Socket"); propIt != AdvancedService<TcpConnectionService>::getProperties().end()) {
            _socket = Ichor::any_cast<int>(propIt->second);
        }

        int setting = 1;
        ::setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, &setting, sizeof(setting));

        timeval timeout{};
        timeout.tv_usec = _sendTimeout;
        setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        ICHOR_LOG_DEBUG(_logger, "[{}] Starting TCP connection for existing socket", AdvancedService<TcpConnectionService>::getServiceId());
    } else {
        auto addrIt = AdvancedService<TcpConnectionService>::getProperties().find("Address");
        auto portIt = AdvancedService<TcpConnectionService>::getProperties().find("Port");

        if(addrIt == AdvancedService<TcpConnectionService>::getProperties().end()) {
            ICHOR_LOG_ERROR(_logger, "[{}] Missing address", AdvancedService<TcpConnectionService>::getServiceId());
            co_return tl::unexpected(StartError::FAILED);
        }
        if(portIt == AdvancedService<TcpConnectionService>::getProperties().end()) {
            ICHOR_LOG_ERROR(_logger, "[{}] Missing port", AdvancedService<TcpConnectionService>::getServiceId());
            co_return tl::unexpected(StartError::FAILED);
        }
        ICHOR_LOG_TRACE(_logger, "[{}] connecting to {}:{}", AdvancedService<TcpConnectionService>::getServiceId(), Ichor::any_cast<std::string&>(addrIt->second), Ichor::any_cast<uint16_t>(portIt->second));

        // The start function possibly gets called multiple times due to trying to recover from not being able to connect
        if(_socket == -1) {
            _socket = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
            if (_socket == -1) {
                throw std::runtime_error("Couldn't create socket: errno = " + std::to_string(errno));
            }
        }

        int setting = 1;
        ::setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, &setting, sizeof(setting));

        timeval timeout{};
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

        bool connected = connect(_socket, (struct sockaddr *)&address, sizeof(address)) < 0;
        while(!connected && _attempts < 5) {
            connected = connect(_socket, (struct sockaddr *)&address, sizeof(address)) < 0;
            if(connected) {
                break;
            }
            ICHOR_LOG_TRACE(_logger, "[{}] connect error {}", AdvancedService<TcpConnectionService>::getServiceId(), errno);
            if(errno == EINPROGRESS) {
                // this is from when the socket was marked as nonblocking, don't think this is necessary anymore.
                pollfd pfd{};
                pfd.fd = _socket;
                pfd.events = POLLOUT;
                ret = poll(&pfd, 1, static_cast<int>(_sendTimeout/1'000));

                if(ret < 0) {
                    ICHOR_LOG_ERROR(_logger, "[{}] poll error {}", AdvancedService<TcpConnectionService>::getServiceId(), errno);
                    continue;
                }

                // timeout
                if(ret == 0) {
                    continue;
                }

                if(pfd.revents & POLLERR) {
                    ICHOR_LOG_ERROR(_logger, "[{}] POLLERR {}", AdvancedService<TcpConnectionService>::getServiceId(), pfd.revents);
                } else if(pfd.revents & POLLHUP) {
                    ICHOR_LOG_ERROR(_logger, "[{}] POLLHUP {}", AdvancedService<TcpConnectionService>::getServiceId(), pfd.revents);
                } else if(pfd.revents & POLLOUT) {
                    int connect_result{};
                    socklen_t result_len = sizeof(connect_result);
                    ret = getsockopt(_socket, SOL_SOCKET, SO_ERROR, &connect_result, &result_len);

                    if(ret < 0) {
                        throw std::runtime_error("getsocketopt error: Couldn't connect");
                    }

                    // connect failed, retry
                    if(connect_result < 0) {
                        ICHOR_LOG_ERROR(_logger, "[{}] POLLOUT {} {}", AdvancedService<TcpConnectionService>::getServiceId(), pfd.revents, connect_result);
                        break;
                    }
                    connected = true;
                    break;
                }
            } else if(errno == EISCONN) {
                connected = true;
                break;
            } else if(errno == EALREADY) {
                std::this_thread::sleep_for(std::chrono::microseconds(_sendTimeout));
            } else {
                _attempts++;
            }
        }

        auto *ip = ::inet_ntoa(address.sin_addr);

        if(!connected) {
            ICHOR_LOG_ERROR(_logger, "[{}] Couldn't start TCP connection for {}:{}", AdvancedService<TcpConnectionService>::getServiceId(), ip, ::ntohs(address.sin_port));
            GetThreadLocalEventQueue().pushEvent<StopServiceEvent>(AdvancedService<TcpConnectionService>::getServiceId(), AdvancedService<TcpConnectionService>::getServiceId(), true);
            co_return tl::unexpected(StartError::FAILED);
        }
        ICHOR_LOG_DEBUG(_logger, "[{}] Starting TCP connection for {}:{}", AdvancedService<TcpConnectionService>::getServiceId(), ip, ::ntohs(address.sin_port));
    }

    _timer = &_timerFactory->createTimer();
    _timer->setFireOnce(true);
    _timer->setChronoInterval(20ms);
    _timer->setCallback([this]() {
        recvHandler();
    });
    _timer->startTimer(true);

    co_return {};
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::IConnectionService, Ichor::IHostConnectionService, Ichor::IClientConnectionService>
Ichor::Task<void> Ichor::TcpConnectionService<InterfaceT>::stop() {
    _quit = true;
    ICHOR_LOG_INFO(_logger, "[{}] stopping service", AdvancedService<TcpConnectionService>::getServiceId());

    if(_socket >= 0) {
        ::shutdown(_socket, SHUT_RDWR);
        ::close(_socket);
    }

    co_return;
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::IConnectionService, Ichor::IHostConnectionService, Ichor::IClientConnectionService>
void Ichor::TcpConnectionService<InterfaceT>::addDependencyInstance(ILogger &logger, IService &) {
    _logger = &logger;
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::IConnectionService, Ichor::IHostConnectionService, Ichor::IClientConnectionService>
void Ichor::TcpConnectionService<InterfaceT>::removeDependencyInstance(ILogger &, IService&) {
    _logger = nullptr;
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::IConnectionService, Ichor::IHostConnectionService, Ichor::IClientConnectionService>
void Ichor::TcpConnectionService<InterfaceT>::addDependencyInstance(ITimerFactory &timerFactory, IService &) {
    _timerFactory = &timerFactory;
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::IConnectionService, Ichor::IHostConnectionService, Ichor::IClientConnectionService>
void Ichor::TcpConnectionService<InterfaceT>::removeDependencyInstance(ITimerFactory &, IService&) {
    _timerFactory = nullptr;
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::IConnectionService, Ichor::IHostConnectionService, Ichor::IClientConnectionService>
Ichor::Task<tl::expected<void, Ichor::IOError>> Ichor::TcpConnectionService<InterfaceT>::sendAsync(std::vector<uint8_t> &&msg) {
    size_t sent_bytes = 0;

    if(_quit) {
        ICHOR_LOG_TRACE(_logger, "[{}] quitting, no send", AdvancedService<TcpConnectionService>::getServiceId());
        co_return tl::unexpected(IOError::SERVICE_QUITTING);
    }

    while(sent_bytes < msg.size()) {
        auto ret = ::send(_socket, msg.data() + sent_bytes, msg.size() - sent_bytes, MSG_NOSIGNAL);
        ICHOR_LOG_TRACE(_logger, "[{}] queued sending {} bytes, errno = {}", AdvancedService<TcpConnectionService>::getServiceId(), ret, errno);

        if(ret < 0) {
            co_return tl::unexpected(IOError::FAILED);
        }

        sent_bytes += static_cast<size_t>(ret);
    }

    co_return {};
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::IConnectionService, Ichor::IHostConnectionService, Ichor::IClientConnectionService>
Ichor::Task<tl::expected<void, Ichor::IOError>> Ichor::TcpConnectionService<InterfaceT>::sendAsync(std::vector<std::vector<uint8_t>> &&msgs) {
    if(_quit) {
        ICHOR_LOG_TRACE(_logger, "[{}] quitting, no send", AdvancedService<TcpConnectionService>::getServiceId());
        co_return tl::unexpected(IOError::SERVICE_QUITTING);
    }

    for(auto &msg : msgs) {
        size_t sent_bytes = 0;

        while(sent_bytes < msg.size()) {
            auto ret = ::send(_socket, msg.data() + sent_bytes, msg.size() - sent_bytes, 0);
            ICHOR_LOG_TRACE(_logger, "[{}] queued sending {} bytes", AdvancedService<TcpConnectionService>::getServiceId(), ret);

            if(ret < 0) {
                co_return tl::unexpected(IOError::FAILED);
            }

            sent_bytes += static_cast<size_t>(ret);
        }
    }

    co_return {};
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::IConnectionService, Ichor::IHostConnectionService, Ichor::IClientConnectionService>
void Ichor::TcpConnectionService<InterfaceT>::setPriority(uint64_t priority) {
    _priority = priority;
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::IConnectionService, Ichor::IHostConnectionService, Ichor::IClientConnectionService>
uint64_t Ichor::TcpConnectionService<InterfaceT>::getPriority() {
    return _priority;
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::IConnectionService, Ichor::IHostConnectionService, Ichor::IClientConnectionService>
bool Ichor::TcpConnectionService<InterfaceT>::isClient() const noexcept {
    return AdvancedService<TcpConnectionService>::getProperties().find("Socket") == AdvancedService<TcpConnectionService>::getProperties().end();
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::IConnectionService, Ichor::IHostConnectionService, Ichor::IClientConnectionService>
void Ichor::TcpConnectionService<InterfaceT>::setReceiveHandler(std::function<void(std::span<uint8_t const>)> recvHandler) {
    _recvHandler = recvHandler;

    for(auto &msg : _queuedMessages) {
        _recvHandler(msg);
    }
    _queuedMessages.clear();
}

template <typename InterfaceT> requires Ichor::DerivedAny<InterfaceT, Ichor::IConnectionService, Ichor::IHostConnectionService, Ichor::IClientConnectionService>
void Ichor::TcpConnectionService<InterfaceT>::recvHandler() {
    ScopeGuard sg{[this]() {
        if(!_quit) {
            if(!_timer->startTimer()) {
                GetThreadLocalEventQueue().pushEvent<RunFunctionEvent>(AdvancedService<TcpConnectionService>::getServiceId(), [this]() {
                    if(!_timer->startTimer()) {
                        std::terminate();
                    }
                });
            }
        } else {
            ICHOR_LOG_TRACE(_logger, "[{}] quitting, no push", AdvancedService<TcpConnectionService>::getServiceId());
        }
    }};
	std::vector<uint8_t> msg{};
	ssize_t ret{};
	{
        std::array<uint8_t, 4096> buf;
		do {
			ret = recv(_socket, buf.data(), buf.size(), MSG_DONTWAIT);
			if (ret > 0) {
				auto data = std::span<uint8_t const>{reinterpret_cast<uint8_t const*>(buf.data()), static_cast<decltype(buf.size())>(ret)};
				msg.insert(msg.end(), data.begin(), data.end());
			}
		} while (ret > 0 && !_quit);
	}
    ICHOR_LOG_TRACE(_logger, "[{}] last received {} bytes, msg size = {}, errno = {}", AdvancedService<TcpConnectionService>::getServiceId(), ret, msg.size(), errno);

    if (_quit) {
        ICHOR_LOG_TRACE(_logger, "[{}] quitting", AdvancedService<TcpConnectionService>::getServiceId());
        return;
    }

	if(!msg.empty()) {
		if(_recvHandler) {
			_recvHandler(msg);
		} else {
			_queuedMessages.emplace_back(msg);
		}
	}

    if(ret == 0) {
        // closed connection
        ICHOR_LOG_INFO(_logger, "[{}] peer closed connection", AdvancedService<TcpConnectionService>::getServiceId());
        GetThreadLocalEventQueue().pushEvent<StopServiceEvent>(AdvancedService<TcpConnectionService>::getServiceId(), AdvancedService<TcpConnectionService>::getServiceId(), true);
        return;
    }

    if(ret < 0) {
		if(errno == EAGAIN) {
			return;
		}
        ICHOR_LOG_ERROR(_logger, "[{}] Error receiving from socket: {}", AdvancedService<TcpConnectionService>::getServiceId(), errno);
        GetThreadLocalEventQueue().pushEvent<StopServiceEvent>(AdvancedService<TcpConnectionService>::getServiceId(), AdvancedService<TcpConnectionService>::getServiceId(), true);
        return;
    }
}

template class Ichor::TcpConnectionService<Ichor::IConnectionService>;
template class Ichor::TcpConnectionService<Ichor::IHostConnectionService>;
template class Ichor::TcpConnectionService<Ichor::IClientConnectionService>;

#endif
