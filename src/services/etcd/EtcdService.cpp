// We're doing some naughty stuff where we tell fmt to insert char's into a vector<uint8_t>.
// Since we have most warnings turned on and -Werror, this turns into compile errors that are actually harmless.
#if defined( __GNUC__ )
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <fmt/core.h>
#if defined( __GNUC__ )
#    pragma GCC diagnostic pop
#endif

#include <ichor/services/etcd/EtcdService.h>
#include <ichor/dependency_management/DependencyRegister.h>

// Glaze uses different conventions than Ichor, ignore them to prevent being spammed by warnings
#if defined( __GNUC__ )
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wsign-conversion"
#    pragma GCC diagnostic ignored "-Wshadow"
#    pragma GCC diagnostic ignored "-Wconversion"
#endif
#include <glaze/glaze.hpp>
#if defined( __GNUC__ )
#    pragma GCC diagnostic pop
#endif

template<>
struct fmt::formatter<glz::error_code> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const glz::error_code& input, FormatContext& ctx) -> decltype(ctx.out()) {
        switch(input) {
            case glz::error_code::none:
                return fmt::format_to(ctx.out(), "glz::error_code::none");
            case glz::error_code::no_read_input:
                return fmt::format_to(ctx.out(), "glz::error_code::no_read_input");
            case glz::error_code::data_must_be_null_terminated:
                return fmt::format_to(ctx.out(), "glz::error_code::data_must_be_null_terminated");
            case glz::error_code::parse_number_failure:
                return fmt::format_to(ctx.out(), "glz::error_code::parse_number_failure");
            case glz::error_code::expected_brace:
                return fmt::format_to(ctx.out(), "glz::error_code::expected_brace");
            case glz::error_code::expected_bracket:
                return fmt::format_to(ctx.out(), "glz::error_code::expected_bracket");
            case glz::error_code::expected_quote:
                return fmt::format_to(ctx.out(), "glz::error_code::expected_quote");
            case glz::error_code::exceeded_static_array_size:
                return fmt::format_to(ctx.out(), "glz::error_code::exceeded_static_array_size");
            case glz::error_code::unexpected_end:
                return fmt::format_to(ctx.out(), "glz::error_code::unexpected_end");
            case glz::error_code::expected_end_comment:
                return fmt::format_to(ctx.out(), "glz::error_code::expected_end_comment");
            case glz::error_code::syntax_error:
                return fmt::format_to(ctx.out(), "glz::error_code::syntax_error");
            case glz::error_code::key_not_found:
                return fmt::format_to(ctx.out(), "glz::error_code::key_not_found");
            case glz::error_code::unexpected_enum:
                return fmt::format_to(ctx.out(), "glz::error_code::unexpected_enum");
            case glz::error_code::attempt_member_func_read:
                return fmt::format_to(ctx.out(), "glz::error_code::attempt_member_func_read");
            case glz::error_code::attempt_read_hidden:
                return fmt::format_to(ctx.out(), "glz::error_code::attempt_read_hidden");
            case glz::error_code::invalid_nullable_read:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_nullable_read");
            case glz::error_code::invalid_variant_object:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_variant_object");
            case glz::error_code::invalid_variant_array:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_variant_array");
            case glz::error_code::invalid_variant_string:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_variant_string");
            case glz::error_code::no_matching_variant_type:
                return fmt::format_to(ctx.out(), "glz::error_code::no_matching_variant_type");
            case glz::error_code::expected_true_or_false:
                return fmt::format_to(ctx.out(), "glz::error_code::expected_true_or_false");
            case glz::error_code::unknown_key:
                return fmt::format_to(ctx.out(), "glz::error_code::unknown_key");
            case glz::error_code::invalid_flag_input:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_flag_input");
            case glz::error_code::invalid_escape:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_escape");
            case glz::error_code::u_requires_hex_digits:
                return fmt::format_to(ctx.out(), "glz::error_code::u_requires_hex_digits");
            case glz::error_code::file_extension_not_supported:
                return fmt::format_to(ctx.out(), "glz::error_code::file_extension_not_supported");
            case glz::error_code::could_not_determine_extension:
                return fmt::format_to(ctx.out(), "glz::error_code::could_not_determine_extension");
            case glz::error_code::seek_failure:
                return fmt::format_to(ctx.out(), "glz::error_code::seek_failure");
            case glz::error_code::unicode_escape_conversion_failure:
                return fmt::format_to(ctx.out(), "glz::error_code::unicode_escape_conversion_failure");
            case glz::error_code::file_open_failure:
                return fmt::format_to(ctx.out(), "glz::error_code::file_open_failure");
            case glz::error_code::file_include_error:
                return fmt::format_to(ctx.out(), "glz::error_code::file_include_error");
            case glz::error_code::dump_int_error:
                return fmt::format_to(ctx.out(), "glz::error_code::dump_int_error");
            case glz::error_code::get_nonexistent_json_ptr:
                return fmt::format_to(ctx.out(), "glz::error_code::get_nonexistent_json_ptr");
            case glz::error_code::get_wrong_type:
                return fmt::format_to(ctx.out(), "glz::error_code::get_wrong_type");
            case glz::error_code::cannot_be_referenced:
                return fmt::format_to(ctx.out(), "glz::error_code::cannot_be_referenced");
            case glz::error_code::invalid_get:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_get");
            case glz::error_code::invalid_get_fn:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_get_fn");
            case glz::error_code::invalid_call:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_call");
            case glz::error_code::invalid_partial_key:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_partial_key");
            case glz::error_code::name_mismatch:
                return fmt::format_to(ctx.out(), "glz::error_code::name_mismatch");
            case glz::error_code::array_element_not_found:
                return fmt::format_to(ctx.out(), "glz::error_code::array_element_not_found");
            case glz::error_code::elements_not_convertible_to_design:
                return fmt::format_to(ctx.out(), "glz::error_code::elements_not_convertible_to_design");
            case glz::error_code::unknown_distribution:
                return fmt::format_to(ctx.out(), "glz::error_code::unknown_distribution");
            case glz::error_code::invalid_distribution_elements:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_distribution_elements");
            case glz::error_code::missing_key:
                return fmt::format_to(ctx.out(), "glz::error_code::missing_key");
            default:
                return fmt::format_to(ctx.out(), "glz::error_code:: ??? report bug!");
        }
    }
};

template <>
struct glz::meta<Ichor::EtcdReplyNode> {
    using T = Ichor::EtcdReplyNode;
    static constexpr auto value = object(
            "createdIndex", &T::createdIndex,
            "modifiedIndex", &T::modifiedIndex,
            "key", &T::key,
            "value", &T::value,
            "ttl", &T::ttl,
            "expiration", &T::expiration,
            "dir", &T::dir,
            "nodes", &T::nodes
    );
};

template <>
struct glz::meta<Ichor::EtcdReply> {
    using T = Ichor::EtcdReply;
    static constexpr auto value = object(
            "action", &T::action,
            "node", &T::node,
            "prevNode", &T::prevNode,
            "index", &T::index,
            "cause", &T::cause,
            "errorCode", &T::errorCode,
            "message", &T::message
    );
};

Ichor::EtcdService::EtcdService(DependencyRegister &reg, Properties props) : AdvancedService<EtcdService>(std::move(props)) {
    reg.registerDependency<ILogger>(this, true);
    reg.registerDependency<IHttpConnectionService>(this, true, getProperties());
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::EtcdService::start() {
    co_return {};
}

Ichor::Task<void> Ichor::EtcdService::stop() {
    co_return;
}

void Ichor::EtcdService::addDependencyInstance(ILogger &logger, IService &) {
    _logger = &logger;
}

void Ichor::EtcdService::removeDependencyInstance(ILogger&, IService&) {
    _logger = nullptr;
}

void Ichor::EtcdService::addDependencyInstance(IHttpConnectionService &conn, IService &) {
    _conn = &conn;
}

void Ichor::EtcdService::removeDependencyInstance(IHttpConnectionService&, IService&) {
    _conn = nullptr;
}

Ichor::Task<tl::expected<Ichor::EtcdReply, Ichor::EtcdError>> Ichor::EtcdService::put(std::string_view key, std::string_view value, std::optional<std::string_view> previous_value, std::optional<uint64_t> previous_index, std::optional<bool> previous_exists, std::optional<uint64_t> ttl_second, bool refresh, bool dir, bool in_order) {
    std::vector<uint8_t> msg_buf;
    fmt::format_to(std::back_inserter(msg_buf), "value={}", value);
    if(ttl_second) {
        fmt::format_to(std::back_inserter(msg_buf), "&ttl={}", *ttl_second);
    }
    if(refresh) {
        fmt::format_to(std::back_inserter(msg_buf), "&refresh=true");
    }
    if(previous_value) {
        fmt::format_to(std::back_inserter(msg_buf), "&prevValue={}", *previous_value);
    }
    if(previous_index) {
        fmt::format_to(std::back_inserter(msg_buf), "&prevIndex={}", *previous_index);
    }
    if(previous_exists) {
        fmt::format_to(std::back_inserter(msg_buf), "&prevExist={}", (*previous_exists) ? "true" : "false");
    }
    if(dir) {
        fmt::format_to(std::back_inserter(msg_buf), "&dir=true");
    }
//    ICHOR_LOG_ERROR(_logger, "put body {}", std::string_view{(char*)msg_buf.data(), msg_buf.size()});
    std::vector<HttpHeader> headers{HttpHeader{"Content-Type", "application/x-www-form-urlencoded"}};
    HttpMethod method = in_order ? HttpMethod::post : HttpMethod::put;
    auto http_reply = co_await _conn->sendAsync(method, fmt::format("/v2/keys/{}", key), std::move(headers), std::move(msg_buf));

    if(http_reply.status != HttpStatus::ok && http_reply.status != HttpStatus::created && http_reply.status != HttpStatus::forbidden) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", fmt::format("/v2/keys/{}", key), (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }
//    ICHOR_LOG_ERROR(_logger, "put json {}", (char*)http_reply.body.data());

    EtcdReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    for(auto &header : http_reply.headers) {
        // TODO refactor headers to unordered_map instead of vector
        if(!header.name.empty() && header.name[0] == 'X') {
            if (header.name == "X-Etcd-Cluster-Id") {
                etcd_reply.x_etcd_cluster_id = header.value;
            }
            if (header.name == "X-Etcd-Index") {
                etcd_reply.x_etcd_index = Ichor::FastAtoiCompare(header.value.c_str());
            }
            if (header.name == "X-Raft-Index") {
                etcd_reply.x_raft_index = Ichor::FastAtoiCompare(header.value.c_str());
            }
            if (header.name == "X-Raft-Term") {
                etcd_reply.x_raft_term = Ichor::FastAtoiCompare(header.value.c_str());
            }
        }
    }

    co_return etcd_reply;
}

Ichor::Task<tl::expected<Ichor::EtcdReply, Ichor::EtcdError>> Ichor::EtcdService::put(std::string_view key, std::string_view value, std::optional<uint64_t> ttl_second, bool refresh) {
    return put(key, value, {}, {}, {}, ttl_second, refresh, false, false);
}

Ichor::Task<tl::expected<Ichor::EtcdReply, Ichor::EtcdError>> Ichor::EtcdService::put(std::string_view key, std::string_view value, std::optional<uint64_t> ttl_second) {
    return put(key, value, {}, {}, {}, ttl_second, false, false, false);
}

Ichor::Task<tl::expected<Ichor::EtcdReply, Ichor::EtcdError>> Ichor::EtcdService::put(std::string_view key, std::string_view value) {
    return put(key, value, {}, {}, {}, {}, false, false, false);
}

Ichor::Task<tl::expected<Ichor::EtcdReply, Ichor::EtcdError>> Ichor::EtcdService::get(std::string_view key, bool recursive, bool sorted) {
    std::string url{fmt::format("/v2/keys/{}?", key)};

    if(recursive) {
        fmt::format_to(std::back_inserter(url), "recursive=true&");
    }
    if(sorted) {
        fmt::format_to(std::back_inserter(url), "sorted=true&");
    }

    auto http_reply = co_await _conn->sendAsync(HttpMethod::get, url, {}, {});

    if(http_reply.status != HttpStatus::ok && http_reply.status != HttpStatus::not_found) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }
//    ICHOR_LOG_ERROR(_logger, "get json {}", (char*)http_reply.body.data());

    EtcdReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}


Ichor::Task<tl::expected<Ichor::EtcdReply, Ichor::EtcdError>> Ichor::EtcdService::get(std::string_view key) {
    return get(key, false, false);
}

Ichor::Task<tl::expected<Ichor::EtcdReply, Ichor::EtcdError>> Ichor::EtcdService::del(std::string_view key, bool recursive, bool dir) {
    std::string url = fmt::format("/v2/keys/{}?", key);
    if(dir) {
        fmt::format_to(std::back_inserter(url), "dir=true&");
    }
    if(recursive) {
        fmt::format_to(std::back_inserter(url), "recursive=true&");
    }
    auto http_reply = co_await _conn->sendAsync(HttpMethod::delete_, url, {}, {});

    if(http_reply.status != HttpStatus::ok) {
        ICHOR_LOG_ERROR(_logger, "Error on route {}, http status {}", url, (int)http_reply.status);
        co_return tl::unexpected(Ichor::EtcdError::HTTP_RESPONSE_ERROR);
    }

    EtcdReply etcd_reply;
    auto err = glz::read_json(etcd_reply, http_reply.body);

    if(err) {
        ICHOR_LOG_ERROR(_logger, "Glaze error {} at {}", err.ec, err.location);
        ICHOR_LOG_ERROR(_logger, "json {}", (char*)http_reply.body.data());
        co_return tl::unexpected(Ichor::EtcdError::JSON_PARSE_ERROR);
    }

    co_return etcd_reply;
}
