#pragma once

#include <ichor/coroutines/Task.h>
#include <ichor/stl/StringUtils.h>
#include <string_view>
#include <optional> // Glaze does not support tl::optional
#include <tl/optional.h>
#include <string>
#include <tl/expected.h>
#include <fmt/core.h>

namespace Ichor::Etcd::v3 {
    enum class EtcdError : uint_fast16_t {
        HTTP_RESPONSE_ERROR,
        VERSION_PARSE_ERROR,
        JSON_PARSE_ERROR,
        TIMEOUT,
        CONNECTION_CLOSED_PREMATURELY_TRY_AGAIN,
        ROOT_USER_NOT_YET_CREATED,
        REMOVING_ROOT_NOT_ALLOWED_WITH_AUTH_ENABLED,
        NO_AUTHENTICATION_SET,
        UNAUTHORIZED,
        NOT_FOUND,
        DUPLICATE_PERMISSION_OR_REVOKING_NON_EXISTENT,
        CANNOT_DELETE_ROOT_WHILE_AUTH_IS_ENABLED,
        QUITTING,
        ETCD_SERVER_DOES_NOT_SUPPORT
    };

    enum class EtcdEventType : uint_fast16_t {
        PUT = 0,
        DELETE_ = 1
    };

    enum class EtcdSortOrder : uint_fast16_t {
        NONE = 0,
        ASCEND = 1,
        DESCEND = 2
    };

    enum class EtcdSortTarget : uint_fast16_t {
        KEY = 0,
        VERSION = 1,
        CREATE = 2,
        MOD = 3,
        VALUE = 4
    };

    struct EtcdKeyValue final {
        std::string key;
        std::string value;
        int64_t create_revision{};
        int64_t mod_revision{};
        int64_t version{};
        int64_t lease{};
    };

    struct EtcdEvent final {
        EtcdEventType type;
        EtcdKeyValue kv;
        EtcdKeyValue prev_kv;
    };

    struct EtcdResponseHeader final {
        uint64_t cluster_id{};
        uint64_t member_id{};
        int64_t revision{};
        uint64_t raft_term{};
    };

    struct EtcdRangeRequest final {
        std::string key;
        std::optional<std::string> range_end;
        int64_t limit{};
        int64_t revision{};
        EtcdSortOrder sort_order;
        EtcdSortTarget sort_target;
        bool serializable{};
        bool keys_only{};
        bool count_only{};
        std::optional<int64_t> min_mod_revision;
        std::optional<int64_t> max_mod_revision;
        std::optional<int64_t> min_create_revision;
        std::optional<int64_t> max_create_revision;
    };

    struct EtcdRangeResponse final {
        EtcdResponseHeader header;
        std::vector<EtcdKeyValue> kvs;
        int64_t count{};
        bool more{};
    };

    struct EtcdPutRequest final {
        std::string key;
        std::string value;
        int64_t lease{};
        std::optional<bool> prev_kv;
        std::optional<bool> ignore_value;
        std::optional<bool> ignore_lease;
    };

    struct EtcdPutResponse final {
        EtcdResponseHeader header;
        std::optional<EtcdKeyValue> prev_kv;

    };

    struct EtcdDeleteRangeRequest final {
        std::string key;
        std::optional<std::string> range_end;
        std::optional<bool> prev_kv;

    };

    struct EtcdDeleteRangeResponse final {
        EtcdResponseHeader header;
        int64_t deleted{};
        std::vector<EtcdKeyValue> prev_kvs;

    };

    struct EtcdTnxRequest final {

    };

    struct EtcdTnxResponse final {

    };

    struct EtcdCompactionRequest final {

    };

    struct EtcdCompactionResponse final {

    };

    struct EtcdWatchRequest final {

    };

    struct EtcdWatchResponse final {

    };

    struct EtcdVersionReply final {
        Version etcdserver;
        Version etcdcluster;
    };

    class IEtcd {
    public:
        /**
         * Create or update or compare-and-swap operation
         *
         * @param req
         * @return Either the EtcdPutResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdPutResponse, EtcdError>> put(EtcdPutRequest const &req) = 0;

        /**
         * Gets (multiple) key(s)
         *
         * @param req
         * @return Either the EtcdRangeResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdRangeResponse, EtcdError>> range(EtcdRangeRequest const &req) = 0;

        /**
         * Delete (multiple) key(s)
         *
         * @param req
         * @return Either the EtcdRangeResponse or an EtcdError
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdDeleteRangeResponse, EtcdError>> deleteRange(EtcdDeleteRangeRequest const &req) = 0;

        /**
         * Get the version of the etcd server that we're connected to
         * @return
         */
        [[nodiscard]] virtual Task<tl::expected<EtcdVersionReply, EtcdError>> version() = 0;

        /**
         * Get the etcd server version that was detected during start.
         * May terminate the program if called before service is started.
         * @return
         */
        [[nodiscard]] virtual Version getDetectedVersion() const = 0;

        /**
         * Get the health status of the etcd server that we're connected to
         * @return
         */
        [[nodiscard]] virtual Task<tl::expected<bool, EtcdError>> health() = 0;

        /**
         * Sets the authentication to use for each request. Caution: might store password in memory.
         * @param user
         * @param password
         */
        virtual void setAuthentication(std::string_view user, std::string_view password) = 0;

        /**
         * Clears used authentication
         */
        virtual void clearAuthentication() = 0;

        /**
         * Gets the user used with the current authentication
         */
        [[nodiscard]] virtual tl::optional<std::string> getAuthenticationUser() const = 0;

    protected:
        ~IEtcd() = default;
    };
}

template <>
struct fmt::formatter<Ichor::Etcd::v3::EtcdError>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::Etcd::v3::EtcdError& state, FormatContext& ctx)
    {
        switch(state)
        {
            case Ichor::Etcd::v3::EtcdError::HTTP_RESPONSE_ERROR:
                return fmt::format_to(ctx.out(), "HTTP_RESPONSE_ERROR");
            case Ichor::Etcd::v3::EtcdError::JSON_PARSE_ERROR:
                return fmt::format_to(ctx.out(), "JSON_PARSE_ERROR");
            case Ichor::Etcd::v3::EtcdError::VERSION_PARSE_ERROR:
                return fmt::format_to(ctx.out(), "VERSION_PARSE_ERROR");
            case Ichor::Etcd::v3::EtcdError::TIMEOUT:
                return fmt::format_to(ctx.out(), "TIMEOUT");
            case Ichor::Etcd::v3::EtcdError::CONNECTION_CLOSED_PREMATURELY_TRY_AGAIN:
                return fmt::format_to(ctx.out(), "CONNECTION_CLOSED_PREMATURELY_TRY_AGAIN");
            case Ichor::Etcd::v3::EtcdError::ROOT_USER_NOT_YET_CREATED:
                return fmt::format_to(ctx.out(), "ROOT_USER_NOT_YET_CREATED");
            case Ichor::Etcd::v3::EtcdError::REMOVING_ROOT_NOT_ALLOWED_WITH_AUTH_ENABLED:
                return fmt::format_to(ctx.out(), "REMOVING_ROOT_NOT_ALLOWED_WITH_AUTH_ENABLED");
            case Ichor::Etcd::v3::EtcdError::NO_AUTHENTICATION_SET:
                return fmt::format_to(ctx.out(), "NO_AUTHENTICATION_SET");
            case Ichor::Etcd::v3::EtcdError::UNAUTHORIZED:
                return fmt::format_to(ctx.out(), "UNAUTHORIZED");
            case Ichor::Etcd::v3::EtcdError::NOT_FOUND:
                return fmt::format_to(ctx.out(), "NOT_FOUND");
            case Ichor::Etcd::v3::EtcdError::DUPLICATE_PERMISSION_OR_REVOKING_NON_EXISTENT:
                return fmt::format_to(ctx.out(), "DUPLICATE_PERMISSION_OR_REVOKING_NON_EXISTENT");
            case Ichor::Etcd::v3::EtcdError::CANNOT_DELETE_ROOT_WHILE_AUTH_IS_ENABLED:
                return fmt::format_to(ctx.out(), "CANNOT_DELETE_ROOT_WHILE_AUTH_IS_ENABLED");
            case Ichor::Etcd::v3::EtcdError::QUITTING:
                return fmt::format_to(ctx.out(), "QUITTING");
            case Ichor::Etcd::v3::EtcdError::ETCD_SERVER_DOES_NOT_SUPPORT:
                return fmt::format_to(ctx.out(), "ETCD_SERVER_DOES_NOT_SUPPORT");
            default:
                return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
        }
    }
};
