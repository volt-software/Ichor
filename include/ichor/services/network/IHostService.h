#pragma once

#include <ichor/Service.h>

namespace Ichor {
    class IHostService {
    public:
        virtual void setPriority(uint64_t priority) = 0;
        virtual uint64_t getPriority() = 0;

    protected:
        ~IHostService() = default;
    };

    class ClientConnectionFilter final {
    public:
        // If a service is requestion IConnection as a means to create a new connection, we don't want it to register for dependencies
        // that are created for incoming connections. If the "Address" key is present in the dependency registration properties, we skip it.
        // Currently only used for WsHostService
        [[nodiscard]] bool matches(Ichor::ILifecycleManager const &manager) const noexcept {
            auto *reg = manager.getDependencyRegistry();

            if(reg == nullptr) {
                return true;
            }

            auto regIt = reg->_registrations.find(Ichor::typeNameHash<Ichor::IConnectionService>());

            if(regIt == reg->_registrations.end()) {
                return true;
            }

            auto props = std::get<3>(regIt->second);

            return !props.has_value() || !props->contains("Address");
        }
    };
}