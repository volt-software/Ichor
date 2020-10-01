#pragma once

#include <cppelix/Service.h>

namespace Cppelix {
    class IConnectionService : public virtual IService {
    public:
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};

        ~IConnectionService() override = default;

        /**
         * Send function. Implementation defined synchronous/asynchronous behaviour.
         * @param msg message to send
         * @return true if succesfully (going to) send message, false otherwise
         */
        virtual bool send(std::vector<uint8_t>&& msg) = 0;

        /**
         * Sets priority with which to push incoming network events.
         * @param priority
         */
        virtual void setPriority(uint64_t priority) = 0;
        virtual uint64_t getPriority() = 0;
    };
}