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

uint64_t Ichor::TcpConnectionService::tcpConnId{};

Ichor::TcpConnectionService::TcpConnectionService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)), _socket(-1), _id(tcpConnId++), _attempts(), _priority(INTERNAL_EVENT_PRIORITY), _quit() {
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
    if(auto propIt = getProperties().find("TimeoutRecvUs"); propIt != getProperties().end()) {
        _recvTimeout = Ichor::any_cast<int64_t>(propIt->second);
    }

    if(getProperties().contains("Socket")) {
        if(auto propIt = getProperties().find("Socket"); propIt != getProperties().end()) {
            _socket = Ichor::any_cast<int>(propIt->second);
        }

        int setting = 1;
        ::setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, &setting, sizeof(setting));

        timeval timeout{};
        timeout.tv_usec = _recvTimeout;
        setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        timeout.tv_usec = _sendTimeout;
        setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        auto flags = ::fcntl(_socket, F_GETFL, 0);
        ::fcntl(_socket, F_SETFL, flags | O_NONBLOCK);
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

        // The start function possibly gets called multiple times due to trying to recover from not being able to connect
        if(_socket == -1) {
            _socket = socket(AF_INET, SOCK_STREAM, 0);
            if (_socket == -1) {
                throw std::runtime_error("Couldn't create socket: errno = " + std::to_string(errno));
            }
        }

        int setting = 1;
        ::setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, &setting, sizeof(setting));

        timeval timeout{};
        timeout.tv_usec = _recvTimeout;
        setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        timeout.tv_usec = _sendTimeout;
        setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        auto flags = ::fcntl(_socket, F_GETFL, 0);
        ::fcntl(_socket, F_SETFL, flags | O_NONBLOCK);

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(Ichor::any_cast<uint16_t>(portIt->second));

        int ret = inet_pton(AF_INET, Ichor::any_cast<std::string&>(addrIt->second).c_str(), &address.sin_addr);
        if(ret == 0)
        {
            throw std::runtime_error("inet_pton invalid address for given address family (has to be ipv4-valid address)");
        }

        bool connected{};
        while(!connected && connect(_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
            ICHOR_LOG_ERROR(_logger, "[{}] connect error {}", _id, errno);
            if(errno == EINPROGRESS) {
                while(_attempts++ >= 5) {
                    pollfd pfd{};
                    pfd.fd = _socket;
                    pfd.events = POLLOUT;
                    ret = poll(&pfd, 1, static_cast<int>(_sendTimeout));

                    if(ret < 0) {
                        ICHOR_LOG_ERROR(_logger, "[{}] poll error {}", _id, errno);
                        continue;
                    }

                    // timeout
                    if(ret == 0) {
                        continue;
                    }

                    if(pfd.revents & POLLERR) {
                        ICHOR_LOG_ERROR(_logger, "[{}] POLLERR {} {} {}", _id, pfd.revents);
                    } else if(pfd.revents & POLLHUP) {
                        ICHOR_LOG_ERROR(_logger, "[{}] POLLHUP {} {} {}", _id, pfd.revents);
                    } else if(pfd.revents & POLLOUT) {
                        int connect_result{};
                        socklen_t result_len = sizeof(connect_result);
                        ret = getsockopt(_socket, SOL_SOCKET, SO_ERROR, &connect_result, &result_len);

                        if(ret < 0) {
                            throw std::runtime_error("getsocketopt error: Couldn't connect");
                        }

                        // connect failed, retry
                        if(connect_result < 0) {
                            break;
                        }
                        connected = true;
                        break;
                    }
                }
            } else if(errno == EALREADY) {
                std::this_thread::sleep_for(std::chrono::microseconds(_sendTimeout));
            } else {
                _attempts++;
            }

            // we don't want to increment attempts in the EINPROGRESS case, but we do want to check it here
            if(_attempts >= 5) {
                throw std::runtime_error("Couldn't connect");
            }
        }

        auto *ip = ::inet_ntoa(address.sin_addr);
        ICHOR_LOG_TRACE(_logger, "[{}] Starting TCP connection for {}:{}", _id, ip, ::ntohs(address.sin_port));
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
        ICHOR_LOG_TRACE(_logger, "[{}] quitting, no send", _id);
        co_return tl::unexpected(IOError::SERVICE_QUITTING);
    }

    while(sent_bytes < msg.size()) {
        auto ret = ::send(_socket, msg.data() + sent_bytes, msg.size() - sent_bytes, 0);

        if(ret < 0) {
            co_return tl::unexpected(IOError::FAILED);
        }

        sent_bytes += static_cast<size_t>(ret);
    }

    co_return {};
}

Ichor::Task<tl::expected<void, Ichor::IOError>> Ichor::TcpConnectionService::sendAsync(std::vector<std::vector<uint8_t>> &&msgs) {
    if(_quit) {
        ICHOR_LOG_TRACE(_logger, "[{}] quitting, no send", _id);
        co_return tl::unexpected(IOError::SERVICE_QUITTING);
    }

    for(auto &msg : msgs) {
        size_t sent_bytes = 0;

        while(sent_bytes < msg.size()) {
            auto ret = ::send(_socket, msg.data() + sent_bytes, msg.size() - sent_bytes, 0);

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
            _timer->startTimer();
        } else {
            ICHOR_LOG_TRACE(_logger, "[{}] quitting, no push", _id);
        }
    }};
	std::vector<uint8_t> msg{};
	int64_t ret{};
	{
        std::array<uint8_t, 1024> buf;
		do {
			ret = recv(_socket, buf.data(), buf.size(), 0);
			if (ret > 0) {
				auto data = std::span<uint8_t const>{reinterpret_cast<uint8_t const*>(buf.data()), static_cast<decltype(buf.size())>(ret)};
				msg.insert(msg.end(), data.begin(), data.end());
			}
		} while (ret > 0 && !_quit);
	}

    if (_quit) {
        ICHOR_LOG_TRACE(_logger, "[{}] quitting", _id);
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
        GetThreadLocalEventQueue().pushEvent<StopServiceEvent>(getServiceId(), getServiceId(), true);
        return;
    }

    if(ret < 0) {
		if(errno == EAGAIN) {
			return;
		}
        ICHOR_LOG_ERROR(_logger, "[{}] Error receiving from socket: {}", _id, errno);
        GetThreadLocalEventQueue().pushEvent<StopServiceEvent>(getServiceId(), getServiceId(), true);
        return;
    }
}

#endif
