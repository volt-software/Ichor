#pragma once

#include <framework/Service.h>

namespace Cppelix {
    class IEtcdService : public virtual IService {
    public:
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};

        ~IEtcdService() override = default;

        virtual bool put(std::string&& key, std::string&& value) = 0;
        virtual std::optional<std::string> get(std::string&& key) = 0;
    };
}