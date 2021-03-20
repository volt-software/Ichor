#pragma once

#include <ichor/Service.h>

using namespace Ichor;

struct UselessService final : public Service<UselessService> {
};