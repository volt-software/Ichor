#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32) && !defined(__APPLE__)) || defined(__CYGWIN__)

#include <ichor/DependencyManager.h>
#include <ichor/services/network/tcp/TcpConnectionService.h>
#include <ichor/services/network/NetworkEvents.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>

Ichor::TcpConnectionService::TcpConnectionService(DependencyRegister &reg, Properties props, DependencyManager *mng) : AdvancedService(std::move(props), mng), _socket(-1), _attempts(), _priority(INTERNAL_EVENT_PRIORITY), _quit() {
    reg.registerDependency<ILogger>(this, true);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::TcpConnectionService::start() {
    if(getProperties().contains("Priority")) {
        _priority = Ichor::any_cast<uint64_t>(getProperties()["Priority"]);
    }


    if(getProperties().contains("Socket")) {
        _socket = Ichor::any_cast<int>(getProperties()["Socket"]);

        ICHOR_LOG_TRACE(_logger, "Starting TCP connection for existing socket");
    } else {
        if(!getProperties().contains("Address")) {
            throw std::runtime_error("Missing \"Address\" in properties");
        }

        if(!getProperties().contains("Port")) {
            throw std::runtime_error("Missing \"Port\" in properties");
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
        auto flags = ::fcntl(_socket, F_GETFL, 0);
        ::fcntl(_socket, F_SETFL, flags | O_NONBLOCK);

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(Ichor::any_cast<uint16_t>((getProperties())["Port"]));

        int ret = inet_pton(AF_INET, Ichor::any_cast<std::string&>((getProperties())["Address"]).c_str(), &address.sin_addr);
        if(ret == 0)
        {
            throw std::runtime_error("inet_pton invalid address for given address family (has to be ipv4-valid address)");
        }

        while(connect(_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
        {
            ICHOR_LOG_ERROR(_logger, "connect error {}", errno);
            if(_attempts++ >= 5) {
                throw std::runtime_error("Couldn't connect");
            }
        }

        auto ip = ::inet_ntoa(address.sin_addr);
        ICHOR_LOG_TRACE(_logger, "Starting TCP connection for {}:{}", ip, ::ntohs(address.sin_port));
    }

    _timerManager = getManager().createServiceManager<Timer, ITimer>();
    _timerManager->setChronoInterval(20ms);
    _timerManager->setCallback(this, [this](DependencyManager &dm) {
        std::array<char, 1024> buf{};
        auto ret = recv(_socket, buf.data(), buf.size(), 0);

        if (ret == 0) {
            return;
        }

        if(ret < 0) {
            ICHOR_LOG_ERROR(_logger, "Error receiving from socket: {}", errno);
            getManager().pushEvent<RecoverableErrorEvent>(getServiceId(), 4u, "Error receiving from socket. errno = " + std::to_string(errno));
            return;
        }

        getManager().pushPrioritisedEvent<NetworkDataEvent>(getServiceId(), _priority, std::vector<uint8_t>{buf.data(), buf.data() + ret});
        return;
    });
    _timerManager->startTimer();

    co_return {};
}

Ichor::Task<void> Ichor::TcpConnectionService::stop() {
    _quit = true;
    _timerManager = nullptr;

    if(_socket >= 0) {
        ::shutdown(_socket, SHUT_RDWR);
        ::close(_socket);
    }

    co_return;
}

void Ichor::TcpConnectionService::addDependencyInstance(ILogger *logger, IService *) {
    _logger = logger;
}

void Ichor::TcpConnectionService::removeDependencyInstance(ILogger *logger, IService *) {
    _logger = nullptr;
}

tl::expected<uint64_t, Ichor::SendErrorReason> Ichor::TcpConnectionService::sendAsync(std::vector<uint8_t> &&msg) {
    auto id = ++_msgIdCounter;
    size_t sent_bytes = 0;

    while(sent_bytes < msg.size()) {
        auto ret = ::send(_socket, msg.data() + sent_bytes, msg.size() - sent_bytes, 0);

        if(ret < 0) {
            getManager().pushEvent<FailedSendMessageEvent>(getServiceId(), std::move(msg), id);
            break;
        }

        sent_bytes += static_cast<uint64_t>(ret);
    }

    return id;
}

void Ichor::TcpConnectionService::setPriority(uint64_t priority) {
    _priority = priority;
}

uint64_t Ichor::TcpConnectionService::getPriority() {
    return _priority;
}

#endif
