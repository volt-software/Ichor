#pragma once

#include <ichor/Service.h>
#include "HttpCommon.h"

namespace Ichor {
    struct HttpResponseEvent final : public Event {
        explicit HttpResponseEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, uint64_t _requestId, HttpResponse&& _response ) noexcept :
                Event(TYPE, NAME, _id, _originatingService, _priority), requestId(_requestId), response(std::forward<HttpResponse>(_response)) {}
        ~HttpResponseEvent() final = default;

        static constexpr uint64_t TYPE = typeNameHash<HttpResponseEvent>();
        static constexpr std::string_view NAME = typeName<HttpResponseEvent>();

        uint64_t requestId;
        HttpResponse response;
    };

    class IHttpConnectionService {
    public:

        /**
         * Send message asynchronously to the connected http server. An HttpResponseEvent is queued when the request finishes.
         * @param method method type (GET, POST, etc)
         * @param route The route, or path, of this request. Has to be pointing to valid memory until an HttpResponseEvent is received.
         * @param msg Usually json, ignored for GET requests
         * @return id of the request, will be available in the HttpResponseEvent
         */
        virtual uint64_t sendAsync(HttpMethod method, std::string_view route, std::vector<HttpHeader> &&headers, std::pmr::vector<uint8_t>&& msg) = 0;

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
        virtual ~IHttpConnectionService() = default;
    };
}
