#pragma once

#include <tl/optional.h>
#include <ichor/stl/StringUtils.h>
#include <ichor/stl/CompilerSpecific.h>

namespace Ichor::v1 {
    ICHOR_CONST_FUNC_ATTR tl::optional<Version> kernelVersion();
}
