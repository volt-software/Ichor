#pragma once

#include <ichor/Service.h>
#include "HttpCommon.h"

namespace Ichor {
    class IHttpConnectionService {
    public:

        /**
         * Send message asynchronously to the connected http server. An HttpResponseEvent is queued when the request finishes. In case of failure, pushes a FailedSendMessageEvent
         * @param method method type (GET, POST, etc)
         * @param route The route, or path, of this request. Has to be pointing to valid memory until an HttpResponseEvent is received.
         * @param msg Usually json, ignored for GET requests
         * @return id of the request, will be available in the HttpResponseEvent
         */
        virtual AsyncGenerator<HttpResponse> sendAsync(HttpMethod method, std::string_view route, std::vector<HttpHeader> &&headers, std::vector<uint8_t>&& msg) = 0;

        /**
         * Close the connection
         * @return true if closed, false if already closed
         */
        virtual bool close() = 0;

        /**
         * Sets priority with which to push incoming network events.
         * @param priority
         */
        virtual void setPriority(uint64_t priority) = 0;
        virtual uint64_t getPriority() = 0;

    protected:
        ~IHttpConnectionService() = default;
    };
}
