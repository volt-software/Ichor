#include <ichor/stl/StringUtils.h>
#include <fmt/format.h>

std::string Ichor::Version::toString() const {
    return fmt::format("{}.{}.{}", major, minor, patch);
}
