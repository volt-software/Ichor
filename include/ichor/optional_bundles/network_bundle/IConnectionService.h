#pragma once

#include <ichor/Service.h>

namespace Ichor {
    class IConnectionService {
    public:
        /**
         * Send function. In case of failure, pushes a FailedSendMessageEvent
         * @param msg message to send
         * @return id of message
         */
        virtual uint64_t sendAsync(std::vector<uint8_t>&& msg) = 0;

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