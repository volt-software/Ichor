#include <ichor/stl/Any.h>
#include <fmt/format.h>

Ichor::bad_any_cast::bad_any_cast(std::string_view type, std::string_view cast) : _error(fmt::format("Bad any_cast. Expected {} but got request to cast to {}", type, cast)) {
}
