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

Ichor::TcpConnectionService::TcpConnectionService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)), _socket(-1), _attempts(), _priority(INTERNAL_EVENT_PRIORITY), _quit() {
    reg.registerDependency<ILogger>(this, DependencyFlags::NONE);
    reg.registerDependency<ITimerFactory>(this, DependencyFlags::REQUIRED);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::TcpConnectionService::start() {
    if(auto propIt = getProperties().find("Priority"); propIt != getProperties().end()) {
        _priority = Ichor::any_cast<uint64_t>(propIt->second);
    }
    if(auto propIt = getProperties().find("TimeoutSendUs"); propIt != getProperties().end()) {
        _sendTimeout = Ichor::any_cast<int64_t>(propIt->second);
    }

    if(getProperties().contains("Socket")) {
        if(auto propIt = getProperties().find("Socket"); propIt != getProperties().end()) {
            _socket = Ichor::any_cast<int>(propIt->second);
        }

        int setting = 1;
        ::setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, &setting, sizeof(setting));

        timeval timeout{};
        timeout.tv_usec = _sendTimeout;
        setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        ICHOR_LOG_DEBUG(_logger, "[{}] Starting TCP connection for existing socket", getServiceId());
    } else {
        auto addrIt = getProperties().find("Address");
        auto portIt = getProperties().find("Port");

        if(addrIt == getProperties().end()) {
            ICHOR_LOG_ERROR(_logger, "[{}] Missing address", getServiceId());
            co_return tl::unexpected(StartError::FAILED);
        }
        if(portIt == getProperties().end()) {
            ICHOR_LOG_ERROR(_logger, "[{}] Missing port", getServiceId());
            co_return tl::unexpected(StartError::FAILED);
        }
        ICHOR_LOG_TRACE(_logger, "[{}] connecting to {}:{}", getServiceId(), Ichor::any_cast<std::string&>(addrIt->second), Ichor::any_cast<uint16_t>(portIt->second));

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
            ICHOR_LOG_TRACE(_logger, "[{}] connect error {}", getServiceId(), errno);
            if(errno == EINPROGRESS) {
                // this is from when the socket was marked as nonblocking, don't think this is necessary anymore.
                pollfd pfd{};
                pfd.fd = _socket;
                pfd.events = POLLOUT;
                ret = poll(&pfd, 1, static_cast<int>(_sendTimeout/1'000));

                if(ret < 0) {
                    ICHOR_LOG_ERROR(_logger, "[{}] poll error {}", getServiceId(), errno);
                    continue;
                }

                // timeout
                if(ret == 0) {
                    continue;
                }

                if(pfd.revents & POLLERR) {
                    ICHOR_LOG_ERROR(_logger, "[{}] POLLERR {}", getServiceId(), pfd.revents);
                } else if(pfd.revents & POLLHUP) {
                    ICHOR_LOG_ERROR(_logger, "[{}] POLLHUP {}", getServiceId(), pfd.revents);
                } else if(pfd.revents & POLLOUT) {
                    int connect_result{};
                    socklen_t result_len = sizeof(connect_result);
                    ret = getsockopt(_socket, SOL_SOCKET, SO_ERROR, &connect_result, &result_len);

                    if(ret < 0) {
                        throw std::runtime_error("getsocketopt error: Couldn't connect");
                    }

                    // connect failed, retry
                    if(connect_result < 0) {
                        ICHOR_LOG_ERROR(_logger, "[{}] POLLOUT {} {}", getServiceId(), pfd.revents, connect_result);
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
            ICHOR_LOG_ERROR(_logger, "[{}] Couldn't start TCP connection for {}:{}", getServiceId(), ip, ::ntohs(address.sin_port));
            GetThreadLocalEventQueue().pushEvent<StopServiceEvent>(getServiceId(), getServiceId(), true);
            co_return tl::unexpected(StartError::FAILED);
        }
        ICHOR_LOG_DEBUG(_logger, "[{}] Starting TCP connection for {}:{}", getServiceId(), ip, ::ntohs(address.sin_port));
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

Ichor::Task<void> Ichor::TcpConnectionService::stop() {
    _quit = true;
    ICHOR_LOG_INFO(_logger, "[{}] stopping service", getServiceId());

    if(_socket >= 0) {
        ::shutdown(_socket, SHUT_RDWR);
        ::close(_socket);
    }

    co_return;
}

void Ichor::TcpConnectionService::addDependencyInstance(ILogger &logger, IService &) {
    _logger = &logger;
}

void Ichor::TcpConnectionService::removeDependencyInstance(ILogger &, IService&) {
    _logger = nullptr;
}

void Ichor::TcpConnectionService::addDependencyInstance(ITimerFactory &timerFactory, IService &) {
    _timerFactory = &timerFactory;
}

void Ichor::TcpConnectionService::removeDependencyInstance(ITimerFactory &, IService&) {
    _timerFactory = nullptr;
}

Ichor::Task<tl::expected<void, Ichor::IOError>> Ichor::TcpConnectionService::sendAsync(std::vector<uint8_t> &&msg) {
    size_t sent_bytes = 0;

    if(_quit) {
        ICHOR_LOG_TRACE(_logger, "[{}] quitting, no send", getServiceId());
        co_return tl::unexpected(IOError::SERVICE_QUITTING);
    }

    while(sent_bytes < msg.size()) {
        auto ret = ::send(_socket, msg.data() + sent_bytes, msg.size() - sent_bytes, MSG_NOSIGNAL);
        ICHOR_LOG_TRACE(_logger, "[{}] queued sending {} bytes, errno = {}", getServiceId(), ret, errno);

        if(ret < 0) {
            co_return tl::unexpected(IOError::FAILED);
        }

        sent_bytes += static_cast<size_t>(ret);
    }

    co_return {};
}

Ichor::Task<tl::expected<void, Ichor::IOError>> Ichor::TcpConnectionService::sendAsync(std::vector<std::vector<uint8_t>> &&msgs) {
    if(_quit) {
        ICHOR_LOG_TRACE(_logger, "[{}] quitting, no send", getServiceId());
        co_return tl::unexpected(IOError::SERVICE_QUITTING);
    }

    for(auto &msg : msgs) {
        size_t sent_bytes = 0;

        while(sent_bytes < msg.size()) {
            auto ret = ::send(_socket, msg.data() + sent_bytes, msg.size() - sent_bytes, 0);
            ICHOR_LOG_TRACE(_logger, "[{}] queued sending {} bytes", getServiceId(), ret);

            if(ret < 0) {
                co_return tl::unexpected(IOError::FAILED);
            }

            sent_bytes += static_cast<size_t>(ret);
        }
    }

    co_return {};
}

void Ichor::TcpConnectionService::setPriority(uint64_t priority) {
    _priority = priority;
}

uint64_t Ichor::TcpConnectionService::getPriority() {
    return _priority;
}

bool Ichor::TcpConnectionService::isClient() const noexcept {
    return getProperties().find("Socket") == getProperties().end();
}

void Ichor::TcpConnectionService::setReceiveHandler(std::function<void(std::span<uint8_t const>)> recvHandler) {
    _recvHandler = recvHandler;

    for(auto &msg : _queuedMessages) {
        _recvHandler(msg);
    }
    _queuedMessages.clear();
}

void Ichor::TcpConnectionService::recvHandler() {
    ScopeGuard sg{[this]() {
        if(!_quit) {
            if(!_timer->startTimer()) {
                GetThreadLocalEventQueue().pushEvent<RunFunctionEvent>(getServiceId(), [this]() {
                    if(!_timer->startTimer()) {
                        std::terminate();
                    }
                });
            }
        } else {
            ICHOR_LOG_TRACE(_logger, "[{}] quitting, no push", getServiceId());
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
    ICHOR_LOG_TRACE(_logger, "[{}] last received {} bytes, msg size = {}, errno = {}", getServiceId(), ret, msg.size(), errno);

    if (_quit) {
        ICHOR_LOG_TRACE(_logger, "[{}] quitting", getServiceId());
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
        ICHOR_LOG_INFO(_logger, "[{}] peer closed connection", getServiceId());
        GetThreadLocalEventQueue().pushEvent<StopServiceEvent>(getServiceId(), getServiceId(), true);
        return;
    }

    if(ret < 0) {
		if(errno == EAGAIN) {
			return;
		}
        ICHOR_LOG_ERROR(_logger, "[{}] Error receiving from socket: {}", getServiceId(), errno);
        GetThreadLocalEventQueue().pushEvent<StopServiceEvent>(getServiceId(), getServiceId(), true);
        return;
    }
}

#endif
