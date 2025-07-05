#include <ichor/stl/StringUtils.h>
#include <fmt/format.h>

std::string Ichor::v1::Version::toString() const {
    return fmt::format("{}.{}.{}", major, minor, patch);
}
