#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include "HttpCommon.h"
#include <ctre/ctre.hpp>

//#define ICHOR_REGEX_DEBUG(...) fmt::print(__VA_ARGS__)
#define ICHOR_REGEX_DEBUG(...)

namespace Ichor {
    class HttpRouteRegistration;
    using RouteIdType = uint64_t;

    struct RouteMatcher {
        virtual ~RouteMatcher() noexcept = default;

        [[nodiscard]] virtual bool matches(std::string_view route) noexcept = 0;
        [[nodiscard]] virtual std::vector<std::string> route_params() noexcept = 0;

        void set_id(RouteIdType id) noexcept {
            _id = id;
        }

        [[nodiscard]] RouteIdType get_id() const noexcept {
            return _id;
        }
    protected:
        RouteIdType _id{};
    };

    struct StringRouteMatcher final : public RouteMatcher {
        StringRouteMatcher(std::string_view route) : _route(route) {}
        ~StringRouteMatcher() noexcept final = default;

        [[nodiscard]] bool matches(std::string_view route) noexcept final {
            ICHOR_REGEX_DEBUG("matcher {} incoming route \"{}\" with matcher route \"{}\"\n", get_id(), route, _route);
            return route == _route;
        }

        [[nodiscard]] std::vector<std::string> route_params() noexcept final {
            return {};
        }

    private:
        std::string_view _route;
    };

    // this relies on the fact that the HTTP spec only allows US ASCII. Any UTF8 regex's will result in UB.
    template <CTRE_REGEX_INPUT_TYPE REGEX>
    std::string ctre_fixed_string_to_std() {
        std::string ret;
        ret.reserve(REGEX.size());
        for(char32_t const &c : REGEX) {
            ret.push_back((char)c);
        }
        return ret;
    }

    template <CTRE_REGEX_INPUT_TYPE REGEX>
    struct RegexRouteMatch final : public RouteMatcher {
        ~RegexRouteMatch() noexcept final = default;

        [[nodiscard]] bool matches(std::string_view route) noexcept final {
            ICHOR_REGEX_DEBUG("matcher {} route \"{}\" with regex \"{}\"\n", get_id(), route, ctre_fixed_string_to_std<REGEX>());
            auto result = ctre::match<REGEX>(route);

            if(!result) {
                ICHOR_REGEX_DEBUG("!result\n");
                return false;
            }

            if constexpr (result.count() == 1) {
                ICHOR_REGEX_DEBUG("result.count() == 1\n");
                return true;
            }

            _params.reserve(result.count() - 1);
            constexpr_for<(size_t)1, result.count(), (size_t)1>([this, &result](auto i) {
                if(!result.template get<i>()) {
                    ICHOR_REGEX_DEBUG("not matched {}\n", (size_t)i);
                    return;
                }

                ICHOR_REGEX_DEBUG("param {} {}\n", (size_t)i, result.template get<i>().to_view());
                _params.emplace_back(result.template get<i>());
            });

            return true;
        }

        [[nodiscard]] std::vector<std::string> route_params() noexcept final {
            return std::move(_params);
        }

    private:
        std::vector<std::string> _params{};
    };

    class IHttpHostService {
    public:
        virtual HttpRouteRegistration addRoute(HttpMethod method, std::string_view route, std::function<AsyncGenerator<HttpResponse>(HttpRequest&)> handler) = 0;
        virtual HttpRouteRegistration addRoute(HttpMethod method, std::unique_ptr<RouteMatcher> matcher, std::function<AsyncGenerator<HttpResponse>(HttpRequest&)> handler) = 0;
        virtual void setPriority(uint64_t priority) = 0;
        virtual uint64_t getPriority() = 0;

    protected:
        virtual void removeRoute(HttpMethod method, RouteIdType id) = 0;
        ~IHttpHostService() = default;

        friend class HttpRouteRegistration;
    };

    class HttpRouteRegistration final {
    public:
        HttpRouteRegistration() = default;
        HttpRouteRegistration(HttpMethod method, RouteIdType id, IHttpHostService *service) : _method(method), _id(id), _service(service) {}
        HttpRouteRegistration(const HttpRouteRegistration&) = delete;
        HttpRouteRegistration(HttpRouteRegistration&& o) noexcept {
            reset();
            _method = o._method;
            _id = o._id;
            _service = o._service;
            o._service = nullptr;
        }

        ~HttpRouteRegistration() {
            reset();
        }

        HttpRouteRegistration& operator=(const HttpRouteRegistration&) = delete;
        HttpRouteRegistration& operator=(HttpRouteRegistration&& o) noexcept {
            reset();
            _method = o._method;
            _id = o._id;
            _service = o._service;
            o._service = nullptr;

            return *this;
        }

        void reset() {
            if(_service != nullptr) {
                _service->removeRoute(_method, _id);
            }
        }
    private:
        HttpMethod _method{};
        RouteIdType _id{};
        IHttpHostService *_service{};
    };
}
