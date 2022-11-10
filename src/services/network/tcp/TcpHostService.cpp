#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)

#include <ichor/DependencyManager.h>
#include <ichor/services/network/IConnectionService.h>
#include <ichor/services/network/tcp/TcpHostService.h>
#include <ichor/services/network/tcp/TcpConnectionService.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>

Ichor::TcpHostService::TcpHostService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng), _socket(-1), _bindFd(), _priority(INTERNAL_EVENT_PRIORITY), _quit() {
    reg.registerDependency<ILogger>(this, true);
}

Ichor::StartBehaviour Ichor::TcpHostService::start() {
    if(getProperties().contains("Priority")) {
        _priority = Ichor::any_cast<uint64_t>(getProperties().operator[]("Priority"));
    }

    _newSocketEventHandlerRegistration = getManager().registerEventHandler<NewSocketEvent>(this);

    _socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if(_socket == -1) {
        getManager().pushEvent<UnrecoverableErrorEvent>(getServiceId(), 0, "Couldn't create socket: errno = " + std::to_string(errno));
        return Ichor::StartBehaviour::FAILED_DO_NOT_RETRY;
    }

    int setting = 1;
    ::setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &setting, sizeof(setting));
    ::setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, &setting, sizeof(setting));
    auto flags = ::fcntl(_socket, F_GETFL, 0);
    ::fcntl(_socket, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in address{};
    address.sin_family = AF_INET;

    auto const addressProp = getProperties().find("Address");

    if(addressProp != cend(getProperties())) {
        auto hostname = Ichor::any_cast<std::string>(addressProp->second);
        if(::inet_aton(hostname.c_str(), &address.sin_addr) != 0) {
            getManager().pushEvent<RecoverableErrorEvent>(getServiceId(), 1, "inet_aton: errno = " + std::to_string(errno));
            auto *hp = ::gethostbyname(hostname.c_str());
            if (hp == nullptr) {
                _socket = -1;
                close(_socket);
                getManager().pushEvent<UnrecoverableErrorEvent>(getServiceId(), 2, "gethostbyname: errno = " + std::to_string(errno));
                return Ichor::StartBehaviour::FAILED_DO_NOT_RETRY;
            }

            memcpy(&address.sin_addr, hp->h_addr, sizeof(address.sin_addr));
        }
    } else {
        address.sin_addr.s_addr = INADDR_ANY;
    }
    address.sin_port = ::htons(Ichor::any_cast<uint16_t>((getProperties())["Port"]));

    _bindFd = ::bind(_socket, (sockaddr *)&address, sizeof(address));

    if(_bindFd == -1) {
        _socket = -1;
        close(_socket);
        getManager().pushEvent<UnrecoverableErrorEvent>(getServiceId(), 3, "Couldn't bind socket: errno = " + std::to_string(errno));
        return Ichor::StartBehaviour::FAILED_DO_NOT_RETRY;
    }

    if(::listen(_socket, 10) != 0) {
        _socket = -1;
        close(_socket);
        getManager().pushEvent<UnrecoverableErrorEvent>(getServiceId(), 4, "Couldn't listen on socket: errno = " + std::to_string(errno));
        return Ichor::StartBehaviour::FAILED_DO_NOT_RETRY;
    }

    _timerManager = getManager().createServiceManager<Timer, ITimer>();
    _timerManager->setChronoInterval(20ms);
    _timerManager->setCallback(this, [this](DependencyManager &dm) -> AsyncGenerator<void> {
        sockaddr_in client_addr{};
        socklen_t client_addr_size = sizeof(client_addr);
        int newConnection = ::accept(_socket, (sockaddr *) &client_addr, &client_addr_size);

        if (newConnection == -1) {
            ICHOR_LOG_ERROR(_logger, "New connection but accept() returned {} errno {}", newConnection, errno);
            if(errno == EINVAL) {
                getManager().pushEvent<UnrecoverableErrorEvent>(getServiceId(), 4, "Accept() generated error. errno = " + std::to_string(errno));
                co_return;
            }
            getManager().pushEvent<RecoverableErrorEvent>(getServiceId(), 4, "Accept() generated error. errno = " + std::to_string(errno));
            co_return;
        }

        auto *ip = ::inet_ntoa(client_addr.sin_addr);
        ICHOR_LOG_TRACE(_logger, "new connection from {}:{}", ip, ::ntohs(client_addr.sin_port));

        getManager().pushPrioritisedEvent<NewSocketEvent>(getServiceId(), _priority, newConnection);
        co_return;
    });
    _timerManager->startTimer();

    return Ichor::StartBehaviour::SUCCEEDED;
}

Ichor::StartBehaviour Ichor::TcpHostService::stop() {
    _quit = true;

    _timerManager = nullptr;

    if(_socket >= 0) {
        ::shutdown(_socket, SHUT_RDWR);
        ::close(_socket);
    }

    _newSocketEventHandlerRegistration.reset();

    return Ichor::StartBehaviour::SUCCEEDED;
}

void Ichor::TcpHostService::addDependencyInstance(ILogger *logger, IService *) {
    _logger = logger;
}

void Ichor::TcpHostService::removeDependencyInstance(ILogger *logger, IService *) {
    _logger = nullptr;
}

void Ichor::TcpHostService::setPriority(uint64_t priority) {
    _priority = priority;
}

uint64_t Ichor::TcpHostService::getPriority() {
    return _priority;
}

Ichor::AsyncGenerator<void> Ichor::TcpHostService::handleEvent(NewSocketEvent const &evt) {
    Properties props{};
    props.emplace("Priority", Ichor::make_any<uint64_t>(_priority));
    props.emplace("Socket", Ichor::make_any<int>(evt.socket));
    _connections.emplace_back(getManager().template createServiceManager<TcpConnectionService, IConnectionService>(std::move(props)));

    co_return;
}

#endif
