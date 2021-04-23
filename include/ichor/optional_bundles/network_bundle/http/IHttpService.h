#pragma once

#include <ichor/Service.h>
#include "HttpCommon.h"

namespace Ichor {
    class HttpRouteRegistration;

    class IHttpService {
    public:
        virtual std::unique_ptr<HttpRouteRegistration, Deleter> addRoute(HttpMethod method, std::string route, std::function<HttpResponse(HttpRequest&)> handler) = 0;
        virtual void removeRoute(HttpMethod method, std::string_view route) = 0;
        virtual void setPriority(uint64_t priority) = 0;
        virtual uint64_t getPriority() = 0;

    protected:
        virtual ~IHttpService() = default;
    };

    class HttpRouteRegistration final {
    public:
        HttpRouteRegistration(HttpMethod method, std::string_view route, IHttpService *service) : _method(method), _route(route), _service(service) {}
        ~HttpRouteRegistration() {
            _service->removeRoute(_method, _route);
        }

    private:
        HttpMethod _method;
        std::string_view _route;
        IHttpService *_service;
    };
}