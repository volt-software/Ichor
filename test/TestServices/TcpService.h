#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/events/RunFunctionEvent.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/network/IConnectionService.h>
#include <thread>
#include <ichor/ScopedServiceProxy.h>

using namespace Ichor;
using namespace Ichor::v1;

extern std::atomic<uint64_t> evtGate;

struct ITcpService {
    virtual Ichor::Task<tl::expected<void, IOError>> sendClientAsync(std::vector<uint8_t>&& msg) = 0;
    virtual Ichor::Task<tl::expected<void, IOError>> sendClientAsync(std::vector<std::vector<uint8_t>>&& msgs) = 0;
    virtual Ichor::Task<tl::expected<void, IOError>> sendHostAsync(std::vector<uint8_t>&& msg) = 0;
    virtual Ichor::Task<tl::expected<void, IOError>> sendHostAsync(std::vector<std::vector<uint8_t>>&& msgs) = 0;
    virtual ServiceIdType getClientId() const noexcept = 0;
    virtual ServiceIdType getHostId() const noexcept = 0;
    virtual std::vector<std::vector<uint8_t>>& getMsgs() noexcept = 0;
protected:
    ~ITcpService() = default;
};

class TcpService final : public ITcpService, public AdvancedService<TcpService> {
public:
    TcpService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<IConnectionService>(this, DependencyFlags::REQUIRED | DependencyFlags::ALLOW_MULTIPLE, getProperties());
    }
    ~TcpService() final = default;

    void addDependencyInstance(Ichor::ScopedServiceProxy<IConnectionService*> connectionService, IService &svc) {
        fmt::println("{} svc injected {} {} {} {} {}", getServiceId(), svc.getServiceId(), connectionService->isClient(), evtGate.load(std::memory_order_acquire), _clientService == nullptr, _hostService == nullptr);
        if(connectionService->isClient()) {
            _clientService = std::move(connectionService);
            _clientId = svc.getServiceId();
        } else {
            _hostService = std::move(connectionService);
            _hostService->setReceiveHandler([this](std::span<uint8_t const> data) {
                fmt::println("svc recv {}", data.size());
                std::string_view fullMsg{reinterpret_cast<char const*>(data.data()), data.size()};
                if(msgs.empty()) {
                    msgs.emplace_back();
                }
                split(fullMsg, "\n", true, [&](std::string_view msg) {
                    msgs.back().insert(msgs.back().end(), msg.begin(), msg.end());
                    if(msg.ends_with('\n')) {
                        msgs.emplace_back();
                        evtGate.fetch_add(1, std::memory_order_acq_rel);
                    }
                });
            });
            _hostId = svc.getServiceId();
        }

        if(_clientService != nullptr && _hostService != nullptr) {
            evtGate.fetch_add(1, std::memory_order_acq_rel);
        }
    }

    void removeDependencyInstance(Ichor::ScopedServiceProxy<IConnectionService*> connectionService, IService& svc) {
        fmt::println("{} svc removed {} {} {} {} {}", getServiceId(), svc.getServiceId(), connectionService->isClient(), evtGate.load(std::memory_order_acquire), _clientService == nullptr, _hostService == nullptr);
        evtGate.fetch_add(1, std::memory_order_acq_rel);
    }

    Ichor::Task<tl::expected<void, IOError>> sendClientAsync(std::vector<uint8_t>&& msg) final {
        co_return co_await _clientService->sendAsync(std::move(msg));
    }

    Ichor::Task<tl::expected<void, IOError>> sendClientAsync(std::vector<std::vector<uint8_t>>&& _msgs) final {
        co_return co_await _clientService->sendAsync(std::move(_msgs));
    }

    Ichor::Task<tl::expected<void, IOError>> sendHostAsync(std::vector<uint8_t>&& msg) final {
        co_return co_await _hostService->sendAsync(std::move(msg));
    }

    Ichor::Task<tl::expected<void, IOError>> sendHostAsync(std::vector<std::vector<uint8_t>>&& _msgs) final {
        co_return co_await _hostService->sendAsync(std::move(_msgs));
    }

    ServiceIdType getClientId() const noexcept final {
        return _clientId;
    }

    ServiceIdType getHostId() const noexcept final {
        return _hostId;
    }

    std::vector<std::vector<uint8_t>>& getMsgs() noexcept final {
        return msgs;
    }

    Ichor::ScopedServiceProxy<IConnectionService*> _clientService {};
    Ichor::ScopedServiceProxy<IConnectionService*> _hostService {};
    std::vector<std::vector<uint8_t>> msgs;
    ServiceIdType _clientId{};
    ServiceIdType _hostId{};
};
