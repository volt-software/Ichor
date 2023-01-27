#pragma once

#include <string>
#include <optional>

namespace Ichor {
    class IEtcdService {
    public:
        virtual bool put(std::string&& key, std::string&& value) = 0;
        virtual std::optional<std::string> get(std::string&& key) = 0;

    protected:
        ~IEtcdService() = default;
    };
}
