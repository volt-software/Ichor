#pragma once

#include <cstdint>
#include <string_view>
#include <sole/sole.h>
#include <ichor/Common.h>
#include <ichor/Enums.h>

namespace Ichor {
    using ServiceIdType = uint64_t;

    class IService {
    public:
        virtual ~IService() = default;

        /// Process-local unique service id
        /// \return id
        [[nodiscard]] virtual ServiceIdType getServiceId() const noexcept = 0;

        /// Global unique service id
        /// \return gid
        [[nodiscard]] virtual sole::uuid getServiceGid() const noexcept = 0;

        /// Name of the user-specified service (e.g. CoutFrameworkLogger)
        /// \return
        [[nodiscard]] virtual std::string_view getServiceName() const noexcept = 0;

        [[nodiscard]] virtual ServiceState getServiceState() const noexcept = 0;

        [[nodiscard]] virtual uint64_t getServicePriority() const noexcept = 0;

        virtual void setServicePriority(uint64_t priority) noexcept = 0;

        [[nodiscard]] virtual bool isStarted() const noexcept = 0;

        [[nodiscard]] virtual Properties& getProperties() noexcept = 0;
        [[nodiscard]] virtual const Properties& getProperties() const noexcept = 0;
    };
}
