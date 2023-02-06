#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <tl/expected.h>

namespace Ichor {
    enum class SendErrorReason {
        QUITTING
    };

    class IConnectionService {
    public:
        /**
         * Send function. In case of failure, pushes a FailedSendMessageEvent
         * @param msg message to send
         * @return id of message
         */
        virtual tl::expected<uint64_t, SendErrorReason> sendAsync(std::vector<uint8_t>&& msg) = 0;

        /**
         * Sets priority with which to push incoming network events.
         * @param priority
         */
        virtual void setPriority(uint64_t priority) = 0;
        virtual uint64_t getPriority() = 0;

    protected:
        ~IConnectionService() = default;
    };
}
