#pragma once

#include <ichor/services/logging/Logger.h>

using namespace Ichor;
using namespace Ichor::v1;

struct IRuntimeCreatedService {
};

class RuntimeCreatedService final : public IRuntimeCreatedService {
public:
    RuntimeCreatedService(IService *self, ILogger *logger) : _logger(logger) {
        auto const& scope = Ichor::v1::any_cast<std::string&>(self->getProperties()["scope"]);
        ICHOR_LOG_INFO(_logger, "RuntimeCreatedService started with scope {}", scope);
    }

    ~RuntimeCreatedService() {
        ICHOR_LOG_INFO(_logger, "RuntimeCreatedService stopped");
    }

private:
    ILogger *_logger{};
};
