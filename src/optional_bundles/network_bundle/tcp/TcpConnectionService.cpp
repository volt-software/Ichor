#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/network_bundle/tcp/TcpConnectionService.h>
#include <ichor/optional_bundles/network_bundle/NetworkEvents.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>

Ichor::TcpConnectionService::TcpConnectionService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng), _socket(-1), _attempts(), _priority(INTERNAL_EVENT_PRIORITY),  _quit() {
    reg.registerDependency<ILogger>(this, true);
}

Ichor::StartBehaviour Ichor::TcpConnectionService::start() {
    if(getProperties().contains("Priority")) {
        _priority = Ichor::any_cast<uint64_t>(getProperties().operator[]("Priority"));
    }


    if(getProperties().contains("Socket")) {
        _socket = Ichor::any_cast<int>(getProperties().operator[]("Socket"));

        ICHOR_LOG_TRACE(_logger, "Starting TCP connection for existing socket");
    } else {
        if(!getProperties().contains("Address")) {
            getManager()->pushEvent<UnrecoverableErrorEvent>(getServiceId(), 0, "Missing \"Address\" in properties");
            return Ichor::StartBehaviour::FAILED_DO_NOT_RETRY;
        }

        if(!getProperties().contains("Port")) {
            getManager()->pushEvent<UnrecoverableErrorEvent>(getServiceId(), 1, "Missing \"Port\" in properties");
            return Ichor::StartBehaviour::FAILED_DO_NOT_RETRY;
        }

        // The start function possibly gets called multiple times due to trying to recover from not being able to connect
        if(_socket == -1) {
            _socket = socket(AF_INET, SOCK_STREAM, 0);
            if (_socket == -1) {
                getManager()->pushEvent<UnrecoverableErrorEvent>(getServiceId(), 2, "Couldn't create socket: errno = " + std::to_string(errno));
                return Ichor::StartBehaviour::FAILED_DO_NOT_RETRY;
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
            getManager()->pushEvent<UnrecoverableErrorEvent>(getServiceId(), 3, "inet_pton invalid address for given address family (has to be ipv4-valid address)");
            return Ichor::StartBehaviour::FAILED_DO_NOT_RETRY;
        }

        if(connect(_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
        {
            ICHOR_LOG_ERROR(_logger, "connect error {}", errno);
            if(_attempts < 5) {
                _attempts++;
                return Ichor::StartBehaviour::FAILED_AND_RETRY;
            }
            return Ichor::StartBehaviour::FAILED_DO_NOT_RETRY;
        }

        auto ip = ::inet_ntoa(address.sin_addr);
        ICHOR_LOG_TRACE(_logger, "Starting TCP connection for {}:{}", ip, ::ntohs(address.sin_port));
    }

    _timerManager = getManager()->createServiceManager<Timer, ITimer>();
    _timerManager->setChronoInterval(20ms);
    _timerManager->setCallback([this](TimerEvent const &evt) -> AsyncGenerator<bool> {
        std::array<char, 1024> buf;
        auto ret = recv(_socket, buf.data(), buf.size(), 0);

        if (ret == 0) {
            co_return (bool)PreventOthersHandling;
        }

        if(ret < 0) {
            ICHOR_LOG_ERROR(_logger, "Error receiving from socket: {}", errno);
            getManager()->pushEvent<RecoverableErrorEvent>(getServiceId(), 4, "Error receiving from socket. errno = " + std::to_string(errno));
            co_return (bool)PreventOthersHandling;
        }

        getManager()->pushPrioritisedEvent<NetworkDataEvent>(getServiceId(), _priority, std::vector<uint8_t>{buf.data(), buf.data() + ret});
        co_return (bool)PreventOthersHandling;
    });
    _timerManager->startTimer();

    return Ichor::StartBehaviour::SUCCEEDED;
}

Ichor::StartBehaviour Ichor::TcpConnectionService::stop() {
    _quit = true;
    _timerManager = nullptr;

    if(_socket >= 0) {
        ::shutdown(_socket, SHUT_RDWR);
        ::close(_socket);
    }

    return Ichor::StartBehaviour::SUCCEEDED;
}

void Ichor::TcpConnectionService::addDependencyInstance(ILogger *logger, IService *) {
    _logger = logger;
}

void Ichor::TcpConnectionService::removeDependencyInstance(ILogger *logger, IService *) {
    _logger = nullptr;
}

uint64_t Ichor::TcpConnectionService::sendAsync(std::vector<uint8_t> &&msg) {
    auto id = ++_msgIdCounter;
    size_t sent_bytes = 0;

    while(sent_bytes < msg.size()) {
        auto ret = ::send(_socket, msg.data() + sent_bytes, msg.size() - sent_bytes, 0);

        if(ret == -1) {
            getManager()->pushEvent<FailedSendMessageEvent>(getServiceId(), std::move(msg), id);
            break;
        }

        sent_bytes += ret;
    }

    return id;
}

void Ichor::TcpConnectionService::setPriority(uint64_t priority) {
    _priority = priority;
}

uint64_t Ichor::TcpConnectionService::getPriority() {
    return _priority;
}
