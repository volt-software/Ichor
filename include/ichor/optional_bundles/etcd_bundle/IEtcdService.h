#pragma once

#include <ichor/Service.h>

namespace Ichor {
    class IEtcdService : public virtual IService {
    public:
        ~IEtcdService() override = default;

        virtual bool put(std::string&& key, std::string&& value) = 0;
        virtual std::optional<std::string> get(std::string&& key) = 0;
    };
}