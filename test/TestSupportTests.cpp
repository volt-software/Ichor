// What the other tests rely on to run in parallel (test/Support)
#include "Common.h"
#include "Support/FreePort.h"

#if !defined(_WIN32)
#include <fstream>

TEST_CASE("TestSupportTests: ephemeralPortRange is the kernel's") {
    std::ifstream file("/proc/sys/net/ipv4/ip_local_port_range");
    uint32_t low{};
    uint32_t high{};
    if(!(file >> low >> high)) {
        SKIP("no /proc/sys/net/ipv4/ip_local_port_range");
    }
    auto const [ephemeralLow, ephemeralHigh] = ephemeralPortRange();
    REQUIRE(ephemeralLow == low);
    REQUIRE(ephemeralHigh == high);
}

TEST_CASE("TestSupportTests: freeTcpPort picks ports outside the ephemeral range") {
    // A port inside it can be taken by another test's outgoing connection before the server binds it, or by the client itself, connecting to it
    // before the server listens: it then connects to itself. Ports 20000-31999 used to be assumed to be outside it, which they're not everywhere.
    auto const [ephemeralLow, ephemeralHigh] = ephemeralPortRange();
    for(int i = 0; i < 1'000; i++) {
        auto const port = freeTcpPort();
        REQUIRE(port >= 1'024);
        REQUIRE((port < ephemeralLow || port > ephemeralHigh));
    }
}
#endif
