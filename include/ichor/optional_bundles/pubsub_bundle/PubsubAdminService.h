#pragma once

#include <ichor/Service.h>
namespace Ichor {
    struct IPubsubAdminService : virtual public IService {
    };

    struct IPubsubPublisherService : virtual public IService {
    };

    struct IPubsubSubscriberService : virtual public IService {
    };
}