//
// Created by oipo on 27-9-24.
//

#include <ichor/stl/LinuxUtils.h>
#include <sys/utsname.h>


tl::optional<Ichor::Version> Ichor::kernelVersion() {
    utsname buffer{};
    if(uname(&buffer) != 0) {
        fmt::println("Couldn't get uname: {}", strerror(errno));
        return {};
    }

    std::string_view release = buffer.release;
    if(auto pos = release.find('-'); pos != std::string_view::npos) {
        release = release.substr(0, pos);
    }

    auto version = parseStringAsVersion(release);

    if(!version) {
        fmt::println("Couldn't parse uname version: {}", buffer.release);
    }

    return version;
}
