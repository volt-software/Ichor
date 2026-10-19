#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/stl/ErrnoUtils.h>
#include "HttpCommon.h"
#include <ctre/ctre.hpp>
#include <tl/expected.h>
#include <functional>
#include <memory>
#include <span>

//#define ICHOR_REGEX_DEBUG(...) fmt::print(__VA_ARGS__)
#define ICHOR_REGEX_DEBUG(...)

namespace Ichor::v1 {
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
        explicit StringRouteMatcher(std::string_view route) : _route(route) {}
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
            ret.push_back(static_cast<char>(c));
        }
        return ret;
    }

    template <CTRE_REGEX_INPUT_TYPE REGEX>
    struct RegexRouteMatch final : public RouteMatcher {
        ~RegexRouteMatch() noexcept final = default;

        [[nodiscard]] bool matches(std::string_view route) noexcept final {
            ICHOR_REGEX_DEBUG("matcher {} route \"{}\" with regex \"{}\"\n", get_id(), route, ctre_fixed_string_to_std<REGEX>());
            // The params of an earlier match whose route_params() were never asked for
            _params.clear();
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
            constexpr_for<static_cast<size_t>(1), result.count(), static_cast<size_t>(1)>([this, &result](auto i) {
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

    /// A connection that is no longer HTTP, after a 101 Switching Protocols or a 2xx response to CONNECT, see IHttpResponseWriter::upgrade()
    class IHttpUpgradedConnection {
    public:
        virtual ~IHttpUpgradedConnection() = default;

        virtual Task<tl::expected<void, IOError>> sendAsync(std::vector<uint8_t> &&msg) = 0;
        /// Called with what the client sends, starting with what it sent after the request that was upgraded
        virtual void setReceiveHandler(std::function<void(std::span<uint8_t const>)> handler) = 0;
        /// Closes the connection
        virtual void close() = 0;
        /// False once the connection is closed, by either side
        [[nodiscard]] virtual bool isOpen() const noexcept = 0;
    };

    /// Sends a response in parts, see IHttpHostService::addStreamingRoute. Only valid while the handler runs.
    class IHttpResponseWriter {
    public:
        /// Sends the status line and header fields of response, and its body as the first part of the content if it has one. The framing is the
        /// host's: chunked for HTTP/1.1 clients, closing the connection after the content for HTTP/1.0 clients. Can only be called once,
        /// write() and the end of the handler start a 200 if it wasn't.
        virtual Task<tl::expected<void, IOError>> start(HttpResponse response) = 0;
        /// Sends data as the next part of the content. Does nothing for responses without content (HEAD, 204, 304).
        virtual Task<tl::expected<void, IOError>> write(std::vector<uint8_t> data) = 0;
        /// After start() with a 101 Switching Protocols (with an Upgrade header for one of the protocols the request's Upgrade header asked for),
        /// or with a 2xx response to CONNECT: the connection is no longer HTTP and is handed over. Nothing if the response can't switch protocols,
        /// or the host can't hand over connections.
        [[nodiscard]] virtual tl::optional<std::shared_ptr<IHttpUpgradedConnection>> upgrade() = 0;

    protected:
        ~IHttpResponseWriter() = default;
    };

    using HttpStreamingHandler = std::function<Task<void>(HttpRequest&, IHttpResponseWriter&)>;

    namespace Detail {
        /// The writer of the default IHttpHostService::addStreamingRoute, which sends the response at once when the handler is done
        class BufferingHttpResponseWriter final : public IHttpResponseWriter {
        public:
            Task<tl::expected<void, IOError>> start(HttpResponse response) final {
                if(_started) {
                    co_return tl::unexpected(IOError::FAILED);
                }
                _started = true;
                _response = std::move(response);
                co_return {};
            }

            Task<tl::expected<void, IOError>> write(std::vector<uint8_t> data) final {
                _started = true;
                _response.body.insert(_response.body.end(), data.begin(), data.end());
                co_return {};
            }

            [[nodiscard]] tl::optional<std::shared_ptr<IHttpUpgradedConnection>> upgrade() final {
                return tl::nullopt;
            }

            [[nodiscard]] HttpResponse take() noexcept {
                return std::move(_response);
            }

        private:
            bool _started{};
            HttpResponse _response{};
        };

        /// A coroutine that owns the handler, which the route could otherwise remove while it runs
        inline Task<HttpResponse> runBuffered(HttpStreamingHandler handler, HttpRequest &req) {
            BufferingHttpResponseWriter writer;
            co_await handler(req, writer);
            co_return writer.take();
        }
    }

    class IHttpHostService {
    public:
        virtual HttpRouteRegistration addRoute(HttpMethod method, std::string_view route, std::function<Task<HttpResponse>(HttpRequest&)> handler) = 0;
        virtual HttpRouteRegistration addRoute(HttpMethod method, std::unique_ptr<RouteMatcher> matcher, std::function<Task<HttpResponse>(HttpRequest&)> handler) = 0;
        /// A route whose handler sends the response in parts through an IHttpResponseWriter, e.g. server-sent events, or takes the connection
        /// over after upgrading it. Hosts that can't stream send the response at once when the handler is done, and can't upgrade.
        virtual HttpRouteRegistration addStreamingRoute(HttpMethod method, std::unique_ptr<RouteMatcher> matcher, HttpStreamingHandler handler);
        virtual HttpRouteRegistration addStreamingRoute(HttpMethod method, std::string_view route, HttpStreamingHandler handler);
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
        HttpRouteRegistration(HttpRouteRegistration&& o) noexcept : _method(o._method), _id(o._id), _service(o._service) {
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

    inline HttpRouteRegistration IHttpHostService::addStreamingRoute(HttpMethod method, std::unique_ptr<RouteMatcher> matcher, HttpStreamingHandler handler) {
        return addRoute(method, std::move(matcher), [handler = std::move(handler)](HttpRequest &req) {
            return Detail::runBuffered(handler, req);
        });
    }

    inline HttpRouteRegistration IHttpHostService::addStreamingRoute(HttpMethod method, std::string_view route, HttpStreamingHandler handler) {
        return addStreamingRoute(method, std::make_unique<StringRouteMatcher>(route), std::move(handler));
    }
}
