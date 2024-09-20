#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32) && !defined(__APPLE__)) || defined(__CYGWIN__)

#include <ichor/DependencyManager.h>
#include <ichor/services/network/IConnectionService.h>
#include <ichor/services/network/tcp/TcpHostService.h>
#include <ichor/services/network/tcp/TcpConnectionService.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/ScopeGuard.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>

Ichor::TcpHostService::TcpHostService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)), _socket(-1), _bindFd(), _priority(INTERNAL_EVENT_PRIORITY), _quit() {
    reg.registerDependency<ILogger>(this, DependencyFlags::NONE);
    reg.registerDependency<ITimerFactory>(this, DependencyFlags::REQUIRED);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::TcpHostService::start() {
    if(auto propIt = getProperties().find("Priority"); propIt != getProperties().end()) {
        _priority = Ichor::any_cast<uint64_t>(propIt->second);
    }

    _newSocketEventHandlerRegistration = GetThreadLocalManager().registerEventHandler<NewSocketEvent>(this, this);

    _socket = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if(_socket == -1) {
        throw std::runtime_error("Couldn't create socket: errno = " + std::to_string(errno));
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
            auto *hp = ::gethostbyname(hostname.c_str());
            if (hp == nullptr) {
                close(_socket);
                _socket = -1;
                throw std::runtime_error("gethostbyname: errno = " + std::to_string(errno));
            }

            memcpy(&address.sin_addr, hp->h_addr, sizeof(address.sin_addr));
        }
    } else {
        address.sin_addr.s_addr = INADDR_ANY;
    }
    address.sin_port = ::htons(Ichor::any_cast<uint16_t>((getProperties())["Port"]));

    _bindFd = ::bind(_socket, (sockaddr *)&address, sizeof(address));

    if(_bindFd == -1) {
        close(_socket);
        _socket = -1;
        throw std::runtime_error("Couldn't bind socket: errno = " + std::to_string(errno));
    }

    if(::listen(_socket, 10) != 0) {
        close(_socket);
        _socket = -1;
        throw std::runtime_error("Couldn't listen on socket: errno = " + std::to_string(errno));
    }

    _timer = &_timerFactory->createTimer();
    _timer->setFireOnce(true);
    _timer->setChronoInterval(20ms);
    _timer->setCallback([this]() {
        acceptHandler();
    });
    _timer->startTimer(true);

    co_return {};
}

Ichor::Task<void> Ichor::TcpHostService::stop() {
    _quit = true;

    if(_socket >= 0) {
        ::shutdown(_socket, SHUT_RDWR);
        ::close(_socket);
    }

    _newSocketEventHandlerRegistration.reset();

    co_return;
}

void Ichor::TcpHostService::addDependencyInstance(ILogger &logger, IService &) {
    _logger = &logger;
}

void Ichor::TcpHostService::removeDependencyInstance(ILogger &, IService&) {
    _logger = nullptr;
}

void Ichor::TcpHostService::addDependencyInstance(ITimerFactory &timerFactory, IService &) {
    _timerFactory = &timerFactory;
}

void Ichor::TcpHostService::removeDependencyInstance(ITimerFactory &, IService&) {
    _timerFactory = nullptr;
}

void Ichor::TcpHostService::setPriority(uint64_t priority) {
    _priority = priority;
}

uint64_t Ichor::TcpHostService::getPriority() {
    return _priority;
}

Ichor::AsyncGenerator<Ichor::IchorBehaviour> Ichor::TcpHostService::handleEvent(NewSocketEvent const &evt) {
    Properties props{};
    props.emplace("Priority", Ichor::make_any<uint64_t>(_priority));
    props.emplace("Socket", Ichor::make_any<int>(evt.socket));
    props.emplace("TimeoutSendUs", Ichor::make_any<int64_t>(_sendTimeout));
    props.emplace("TimeoutRecvUs", Ichor::make_any<int64_t>(_recvTimeout));
    _connections.emplace_back(GetThreadLocalManager().template createServiceManager<TcpConnectionService, IConnectionService>(std::move(props))->getServiceId());

    co_return {};
}

void Ichor::TcpHostService::acceptHandler() {
    sockaddr_in client_addr{};
    socklen_t client_addr_size = sizeof(client_addr);
    ScopeGuard sg{[this]() {
        if(!_quit) {
            _timer->startTimer();
        } else {
            ICHOR_LOG_TRACE(_logger, "quitting, no push");
        }
    }};
    int newConnection = ::accept(_socket, (sockaddr *) &client_addr, &client_addr_size);

    if(_quit) {
        ICHOR_LOG_TRACE(_logger, "quitting");
        return;
    }

    if (newConnection == -1) {
		if(errno == EAGAIN) {
			return;
		}
        ICHOR_LOG_ERROR(_logger, "New connection but accept() returned {} errno {}", newConnection, errno);
        GetThreadLocalEventQueue().pushEvent<StopServiceEvent>(getServiceId(), getServiceId(), true);
        return;
    }

    auto *ip = ::inet_ntoa(client_addr.sin_addr);
    ICHOR_LOG_TRACE(_logger, "new connection from {}:{}", ip, ::ntohs(client_addr.sin_port));

    GetThreadLocalEventQueue().pushPrioritisedEvent<NewSocketEvent>(getServiceId(), _priority, newConnection);
}

#endif
