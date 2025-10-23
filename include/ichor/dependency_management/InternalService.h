#pragma once

#include <ichor/dependency_management/IService.h>

namespace Ichor::Detail {

    /// InternalService is meant to be used for Ichor internal services. This service does not actually own a service like the regular AdvancedService/ConstructorInjectionService, but "pretends" to be a service.
    /// This is especially useful for queues and the DM itself, as they are created outside of the regular dependency mechanism, but we still want services to request these as dependencies.
    /// \tparam OriginalType
    template <typename OriginalType>
    class InternalService : public IService {
    public:
        InternalService() noexcept : IService(), _properties(), _serviceId(ServiceIdType{Detail::_serviceIdCounter.fetch_add(1, std::memory_order_relaxed)}), _servicePriority(INTERNAL_EVENT_PRIORITY), _serviceGid(sole::uuid4()), _serviceState(ServiceState::INSTALLED) {
        }

        ~InternalService() noexcept override {
            _serviceState = ServiceState::UNINSTALLED;
        }

        InternalService(const InternalService&) = default;
        InternalService(InternalService&&) noexcept = default;
        InternalService& operator=(const InternalService&) = default;
        InternalService& operator=(InternalService&&) noexcept = default;

        /// Process-local unique service id
        /// \return id
        [[nodiscard]] ICHOR_PURE_FUNC_ATTR ServiceIdType getServiceId() const noexcept final {
            return _serviceId;
        }

        /// Global unique service id
        /// \return gid
        [[nodiscard]] ICHOR_PURE_FUNC_ATTR sole::uuid getServiceGid() const noexcept final {
            return _serviceGid;
        }

        /// Name of the user-specified service (e.g. CoutFrameworkLogger)
        /// \return
        [[nodiscard]] ICHOR_PURE_FUNC_ATTR std::string_view getServiceName() const noexcept final {
            return typeName<OriginalType>();
        }

        [[nodiscard]] ServiceState getServiceState() const noexcept final {
            return _serviceState;
        }

        [[nodiscard]] uint64_t getServicePriority() const noexcept final {
            return _servicePriority;
        }

        void setServicePriority(uint64_t priority) noexcept final {
            _servicePriority = priority;
        }

        [[nodiscard]] bool isStarted() const noexcept final {
            return _serviceState == ServiceState::ACTIVE;
        }

        [[nodiscard]] Properties& getProperties() noexcept final {
            return _properties;
        }

        [[nodiscard]] const Properties& getProperties() const noexcept final {
            return _properties;
        }

        [[nodiscard]] ServiceState getState() noexcept {
            return _serviceState;
        }

        void setState(ServiceState state) noexcept {
            _serviceState = state;
        }

    protected:
        Properties _properties;
    private:

        ServiceIdType _serviceId;
        uint64_t _servicePriority;
        sole::uuid _serviceGid;
        ServiceState _serviceState;
    };
}
