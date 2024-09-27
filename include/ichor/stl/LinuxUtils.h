#pragma once

#include <tl/optional.h>
#include <ichor/stl/StringUtils.h>

namespace Ichor {
    tl::optional<Ichor::Version> kernelVersion();
}
