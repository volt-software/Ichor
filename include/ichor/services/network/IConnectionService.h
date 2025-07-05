#pragma once

#include <ichor/stl/ErrnoUtils.h>
#include <tl/expected.h>
#include <vector>
#include <span>

namespace Ichor::v1 {
    class IConnectionService {
    public:
        /**
         * Awaitable send function.
         * @param msg message to send
         * @return void on success, IOError otherwise
         */
        virtual Ichor::Task<tl::expected<void, IOError>> sendAsync(std::vector<uint8_t>&& msg) = 0;
        /**
         * Awaitable send function for multiple messages. Some implementations may use iovec to send all messages in one go.
         * @param msgs messages to send
         * @return void on success, IOError otherwise
         */
        virtual Ichor::Task<tl::expected<void, IOError>> sendAsync(std::vector<std::vector<uint8_t>>&& msgs) = 0;
        /**
         * Send function with callback, in case many messages have to be sent without awaiting.
         * @param msg message to send
         * @param cb callback with void on success and IOError on failure
         * @return void on success, IOError on failure putting send in the queue.
         */
//        virtual tl::expected<void, IOError> sendCallback(std::vector<uint8_t>&& msg, std::function<void(tl::expected<void, IOError>)> cb) = 0;

        /**
         * Sets priority with which to push incoming network events.
         * @param priority
         */
        virtual void setPriority(uint64_t priority) = 0;
        virtual uint64_t getPriority() = 0;

        [[nodiscard]] virtual bool isClient() const noexcept = 0;

        virtual void setReceiveHandler(std::function<void(std::span<uint8_t const>)>) = 0;

    protected:
        ~IConnectionService() = default;
    };

    struct IHostConnectionService : public IConnectionService {
        using IConnectionService::IConnectionService;

    protected:
        ~IHostConnectionService() = default;
    };

    struct IClientConnectionService : public IConnectionService {
        using IConnectionService::IConnectionService;

    protected:
        ~IClientConnectionService() = default;
    };
}
