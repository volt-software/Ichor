#pragma once

#include <ichor/services/network/IConnectionService.h>

template <typename T>
struct ConnectionServiceMock : public T, public AdvancedService<ConnectionServiceMock<T>> {
    ConnectionServiceMock(DependencyRegister &reg, Properties props) : AdvancedService<ConnectionServiceMock<T>>(std::move(props)) {}
    Ichor::Task<tl::expected<void, IOError>> sendAsync(std::vector<uint8_t>&& msg) override {
        sentMessages.emplace_back(std::move(msg));
        co_return {};
    }

    Ichor::Task<tl::expected<void, IOError>> sendAsync(std::vector<std::vector<uint8_t>>&& msgs) override {
        sentMessages.insert(sentMessages.end(), msgs.begin(), msgs.end());
        co_return {};
    }

    void setPriority(uint64_t priority) override {

    }

    uint64_t getPriority() override {
        return 0;
    }

    [[nodiscard]] bool isClient() const noexcept override {
        return is_client;
    }

    void setReceiveHandler(std::function<void(std::span<uint8_t const>)> _rcvHandler) override {
        rcvHandler = std::move(_rcvHandler);
    }

    std::vector<std::vector<uint8_t>> sentMessages;
    std::function<void(std::span<uint8_t const>)> rcvHandler;
    bool is_client{};
};
