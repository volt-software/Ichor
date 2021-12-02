#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/network_bundle/tcp/TcpConnectionService.h>
#include <ichor/optional_bundles/network_bundle/NetworkDataEvent.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>

Ichor::TcpConnectionService::TcpConnectionService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng), _socket(-1), _attempts(), _priority(INTERNAL_EVENT_PRIORITY),  _quit() {
    reg.registerDependency<ILogger>(this, true);
}

bool Ichor::TcpConnectionService::start() {
    if(getProperties()->contains("Priority")) {
        _priority = Ichor::any_cast<uint64_t>(getProperties()->operator[]("Priority"));
    }


    if(getProperties()->contains("Socket")) {
        _socket = Ichor::any_cast<int>(getProperties()->operator[]("Socket"));

        ICHOR_LOG_TRACE(_logger, "Starting TCP connection for existing socket");
    } else {
        if(!getProperties()->contains("Address")) {
            getManager()->pushEvent<UnrecoverableErrorEvent>(getServiceId(), 0, "Missing \"Address\" in properties");
            return false;
        }

        if(!getProperties()->contains("Port")) {
            getManager()->pushEvent<UnrecoverableErrorEvent>(getServiceId(), 1, "Missing \"Port\" in properties");
            return false;
        }

        // The start function possibly gets called multiple times due to trying to recover from not being able to connect
        if(_socket == -1) {
            _socket = socket(AF_INET, SOCK_STREAM, 0);
            if (_socket == -1) {
                getManager()->pushEvent<UnrecoverableErrorEvent>(getServiceId(), 2, "Couldn't create socket: errno = " + std::to_string(errno));
                return false;
            }
        }

        int setting = 1;
        ::setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, &setting, sizeof(setting));
        auto flags = ::fcntl(_socket, F_GETFL, 0);
        ::fcntl(_socket, F_SETFL, flags | O_NONBLOCK);

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(Ichor::any_cast<uint16_t>((*getProperties())["Port"]));

        int ret = inet_pton(AF_INET, Ichor::any_cast<std::string&>((*getProperties())["Address"]).c_str(), &address.sin_addr);
        if(ret == 0)
        {
            getManager()->pushEvent<UnrecoverableErrorEvent>(getServiceId(), 3, "inet_pton invalid address for given address family (has to be ipv4-valid address)");
            return false;
        }

        if(connect(_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
        {
            ICHOR_LOG_ERROR(_logger, "connect error {}", errno);
            if(_attempts < 5) {
                _attempts++;
                getManager()->pushEvent<StartServiceEvent>(getServiceId(), getServiceId());
            }
            return false;
        }

        auto ip = ::inet_ntoa(address.sin_addr);
        ICHOR_LOG_TRACE(_logger, "Starting TCP connection for {}:{}", ip, ::ntohs(address.sin_port));
    }

    _timerManager = getManager()->createServiceManager<Timer, ITimer>();
    _timerManager->setChronoInterval(std::chrono::milliseconds(20));
    _timerManager->setCallback([this](TimerEvent const * const evt) -> Generator<bool> {
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

        getManager()->pushPrioritisedEvent<NetworkDataEvent>(getServiceId(), _priority, std::vector<uint8_t, Ichor::PolymorphicAllocator<uint8_t>>{buf.data(), buf.data() + ret, getMemoryResource()});
        co_return (bool)PreventOthersHandling;
    });
    _timerManager->startTimer();

    return true;
}

bool Ichor::TcpConnectionService::stop() {
    _quit = true;
    _timerManager = nullptr;

    if(_socket >= 0) {
        ::shutdown(_socket, SHUT_RDWR);
        ::close(_socket);
    }

    return true;
}

void Ichor::TcpConnectionService::addDependencyInstance(ILogger *logger, IService *) {
    _logger = logger;
    ICHOR_LOG_TRACE(_logger, "Inserted logger");
}

void Ichor::TcpConnectionService::removeDependencyInstance(ILogger *logger, IService *) {
    _logger = nullptr;
}

bool Ichor::TcpConnectionService::send(std::vector<uint8_t, Ichor::PolymorphicAllocator<uint8_t>> &&msg) {
    size_t sent_bytes = 0;
    while(sent_bytes < msg.size()) {
        auto ret = ::send(_socket, msg.data() + sent_bytes, msg.size() - sent_bytes, 0);

        if(ret == -1) {
            throw std::runtime_error(fmt::format("Error sending: {}", errno));
        }

        sent_bytes += ret;
    }

    return true;
}

void Ichor::TcpConnectionService::setPriority(uint64_t priority) {
    _priority = priority;
}

uint64_t Ichor::TcpConnectionService::getPriority() {
    return _priority;
}
