#pragma once

#include <ichor/Service.h>

using namespace Ichor;

struct IUselessService : virtual public IService {
};
struct UselessService final : public IUselessService, public Service<UselessService> {
};