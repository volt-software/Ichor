#pragma once

#include <cstdint>

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
/// The tests don't run in parallel on Windows
inline uint16_t freeTcpPort() {
    return 8001;
}
#else
#include <cstdio>
#include <cstdlib>
#include <fmt/base.h>
#include <fstream>
#include <random>
#include <utility>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

/// The ports Linux picks the local port of an outgoing connection from (net.ipv4.ip_local_port_range), or Linux's default when that can't be read
inline std::pair<uint16_t, uint16_t> ephemeralPortRange() {
    std::ifstream file("/proc/sys/net/ipv4/ip_local_port_range");
    uint32_t low{};
    uint32_t high{};
    if(file >> low >> high && low <= high && high <= 65'535) {
        return {static_cast<uint16_t>(low), static_cast<uint16_t>(high)};
    }
    return {32'768, 60'999};
}

/// A TCP port on 127.0.0.1 that nothing listens on, for a test's server, so that tests running at the same time don't use the same one.
/// It's picked from outside the ephemeral range, so that it can't become the local port of an outgoing connection: neither one of another test, nor
/// that of a client trying to connect to it before the server listens (which would connect to itself). Only another test picking the same port
/// between this check and its server binding it could take it.
inline uint16_t freeTcpPort() {
    // binding the ones below takes privileges
    constexpr uint32_t lowestPort = 1'024;
    auto const [ephemeralLow, ephemeralHigh] = ephemeralPortRange();
    // the ports [lowestPort, ephemeralLow) and (ephemeralHigh, 65535]
    uint32_t const below = ephemeralLow > lowestPort ? ephemeralLow - lowestPort : 0;
    uint32_t const above = 65'535u - ephemeralHigh;
    if(below + above == 0) {
        fmt::println(stderr, "freeTcpPort: the ephemeral port range {}-{} leaves no ports for the tests' servers", ephemeralLow, ephemeralHigh);
        std::abort();
    }

    std::random_device rd;
    std::uniform_int_distribution<uint32_t> distribution{0, below + above - 1};
    for(int attempt = 0; attempt < 1'000; attempt++) {
        auto const pick = distribution(rd);
        auto const port = static_cast<uint16_t>(pick < below ? lowestPort + pick : ephemeralHigh + 1 + (pick - below));
        int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if(fd < 0) {
            continue;
        }
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bool const free = ::bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0 && ::listen(fd, 1) == 0;
        ::close(fd);
        if(free) {
            return port;
        }
    }
    // a server given port 0 would listen on a random one, which its client wouldn't know
    fmt::println(stderr, "freeTcpPort: found no free port outside the ephemeral port range {}-{}", ephemeralLow, ephemeralHigh);
    std::abort();
}
#endif
