#pragma once

#include <ichor/coroutines/Task.h>
#include <string_view>
#include <optional>
#include <string>
#include <tl/expected.h>
#include <fmt/core.h>

namespace Ichor {
    enum class EtcdError : uint64_t {
        HTTP_RESPONSE_ERROR,
        JSON_PARSE_ERROR,
        TIMEOUT,
        CONNECTION_CLOSED_PREMATURELY_TRY_AGAIN,
        QUITTING
    };

    enum class EtcdErrorCodes : uint64_t {
        KEY_DOES_NOT_EXIST = 100,
        COMPARE_AND_SWAP_FAILED = 101,
        NOT_A_FILE = 102,
        NOT_A_DIRECTORY = 104,
        KEY_ALREADY_EXISTS = 105,
        ROOT_IS_READ_ONLY = 107,
        DIRECTORY_NOT_EMPTY = 108,
        PREVIOUS_VALUE_REQUIRED = 201,
        TTL_IS_NOT_A_NUMBER = 202,
        INDEX_IS_NOT_A_NUMBER = 203,
        INVALID_FIELD = 209,
        INVALID_FORM = 210,
        RAFT_INTERNAL_ERROR = 300,
        LEADER_ELECTRION_ERROR = 301,
        WATCHER_CLEARED_DUE_TO_RECOVERY = 400,
        EVENT_INDEX_OUTDATED_AND_CLEARED = 401,
    };

    struct EtcdReplyNode final {
        uint64_t createdIndex;
        uint64_t modifiedIndex;
        std::string key;
        std::optional<std::string> value;
        std::optional<uint64_t> ttl;
        std::optional<std::string> expiration;
        std::optional<bool> dir;
        std::optional<std::vector<EtcdReplyNode>> nodes;
    };

    struct EtcdReply final {
        std::optional<std::string> action;
        std::optional<EtcdReplyNode> node;
        std::optional<EtcdReplyNode> prevNode;
        std::optional<uint64_t> index;

        std::optional<std::string> cause;
        std::optional<EtcdErrorCodes> errorCode;
        std::optional<std::string> message;

        std::optional<std::string> x_etcd_cluster_id;
        std::optional<uint64_t> x_etcd_index;
        std::optional<uint64_t> x_raft_index;
        std::optional<uint64_t> x_raft_term;
    };

    struct EtcdLeaderInfoStats final {
        std::string leader;
        std::string uptime;
        std::string startTime;
    };

    struct EtcdSelfStats final {
        std::string name;
        std::string id;
        std::string state;
        std::string startTime;
        EtcdLeaderInfoStats leaderInfo;
        uint64_t recvAppendRequestCnt;
        uint64_t sendAppendRequestCnt;
    };

    struct EtcdStoreStats final {
        uint64_t compareAndSwapFail;
        uint64_t compareAndSwapSuccess;
        uint64_t compareAndDeleteSuccess;
        uint64_t compareAndDeleteFail;
        uint64_t createFail;
        uint64_t createSuccess;
        uint64_t deleteFail;
        uint64_t deleteSuccess;
        uint64_t expireCount;
        uint64_t getsFail;
        uint64_t getsSuccess;
        uint64_t setsFail;
        uint64_t setsSuccess;
        uint64_t updateFail;
        uint64_t updateSuccess;
        uint64_t watchers;
    };

    struct EtcdVersionReply final {
        std::string etcdserver;
        std::string etcdcluster;
    };

    class IEtcd {
    public:
        /**
         * Create or update or compare-and-swap operation
         *
         * @param key key to update/create
         * @param value value to set to
         * @param previous_value optional: set to true to only update the key if the current value equals previous_value, leave nullopt to get the default etcd behaviour
         * @param previous_index optional: set to a specific to only update the key if the current indedx equals previous_index, leave nullopt or set to 0 get the default etcd behaviour
         * @param previous_exists optional: set to true to only update the key if the key already exists, set to false to only create a key and not update, leave nullopt to enable both
         * @param ttl_second optional: how many seconds should this key last
         * @param refresh set to true to only refresh the TTL or value
         * @param dir set to true to create/update a directory
         * @param in_order set to true to change to a POST request and create in-order keys, most commonly used for queues.
         * @return Either the EtcdReply or an EtcdError
         */
        virtual Task<tl::expected<EtcdReply, EtcdError>> put(std::string_view key, std::string_view value, std::optional<std::string_view> previous_value, std::optional<uint64_t> previous_index, std::optional<bool> previous_exists, std::optional<uint64_t> ttl_seconds, bool refresh, bool dir, bool in_order) = 0;
        /**
         * Create or update operation
         *
         * @param key key to update/create
         * @param value value to set to
         * @param ttl_second optional: how many seconds should this key last
         * @param refresh set to true to only refresh the TTL or value
         * @return Either the EtcdReply or an EtcdError
         */
        virtual Task<tl::expected<EtcdReply, EtcdError>> put(std::string_view key, std::string_view value, std::optional<uint64_t> ttl_seconds, bool refresh) = 0;
        /**
         * Create or update operation
         *
         * @param key key to update/create
         * @param value value to set to
         * @param ttl_second optional: how many seconds should this key last
         * @return Either the EtcdReply or an EtcdError
         */
        virtual Task<tl::expected<EtcdReply, EtcdError>> put(std::string_view key, std::string_view value, std::optional<uint64_t> ttl_seconds) = 0;
        /**
         * Create or update operation
         *
         * @param key key to update/create
         * @param value value to set to
         * @return Either the EtcdReply or an EtcdError
         */
        virtual Task<tl::expected<EtcdReply, EtcdError>> put(std::string_view key, std::string_view value) = 0;

        /**
         * Get value for key operation
         *
         * @param key
         * @param recursive Set to true if key is a directory and you want all underlying keys/values
         * @param sorted Set to true if
         * @return Either the EtcdReply or an EtcdError
         */
        virtual Task<tl::expected<EtcdReply, EtcdError>> get(std::string_view key, bool recursive, bool sorted, bool watch, std::optional<uint64_t> watchIndex) = 0;

        /**
         * Get value for key operation
         *
         * @param key
         * @return Either the EtcdReply or an EtcdError
         */
        virtual Task<tl::expected<EtcdReply, EtcdError>> get(std::string_view key) = 0;

        /**
         * Delete key operation
         *
         * @param key
         * @param recursive set to true if key is a directory and you want to recursively delete all keys. If dir has keys but delete is not recursive, delete will fail.
         * @param dir set to true if key is a directory
         * @return Either the EtcdReply or an EtcdError
         */
        virtual Task<tl::expected<EtcdReply, EtcdError>> del(std::string_view key, bool recursive, bool dir) = 0;

        /**
         * Unimplemented
         * @return
         */
        virtual Task<tl::expected<EtcdReply, EtcdError>> leaderStatistics() = 0;

        /**
         * Get statistics of the etcd server that we're connected to
         * @return
         */
        virtual Task<tl::expected<EtcdSelfStats, EtcdError>> selfStatistics() = 0;

        /**
         * Get the store statistics of the etcd server that we're connected to
         * @return
         */
        virtual Task<tl::expected<EtcdStoreStats, EtcdError>> storeStatistics() = 0;

        /**
         * Get the version of the etcd server that we're connected to
         * @return
         */
        virtual Task<tl::expected<EtcdVersionReply, EtcdError>> version() = 0;

        /**
         * Get the health status of the etcd server that we're connected to
         * @return
         */
        virtual Task<tl::expected<bool, EtcdError>> health() = 0;



    protected:
        ~IEtcd() = default;
    };
}

template <>
struct fmt::formatter<Ichor::EtcdError>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::EtcdError& state, FormatContext& ctx)
    {
        switch(state)
        {
            case Ichor::EtcdError::JSON_PARSE_ERROR:
                return fmt::format_to(ctx.out(), "JSON_PARSE_ERROR");
            case Ichor::EtcdError::HTTP_RESPONSE_ERROR:
                return fmt::format_to(ctx.out(), "HTTP_RESPONSE_ERROR");
            case Ichor::EtcdError::TIMEOUT:
                return fmt::format_to(ctx.out(), "TIMEOUT");
            default:
                return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
        }
    }
};

template <>
struct fmt::formatter<Ichor::EtcdErrorCodes>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::EtcdErrorCodes& state, FormatContext& ctx)
    {
        switch(state)
        {
            case Ichor::EtcdErrorCodes::KEY_DOES_NOT_EXIST:
                return fmt::format_to(ctx.out(), "KEY_DOES_NOT_EXIST");
            case Ichor::EtcdErrorCodes::COMPARE_AND_SWAP_FAILED:
                return fmt::format_to(ctx.out(), "COMPARE_AND_SWAP_FAILED");
            case Ichor::EtcdErrorCodes::NOT_A_FILE:
                return fmt::format_to(ctx.out(), "NOT_A_FILE");
            case Ichor::EtcdErrorCodes::NOT_A_DIRECTORY:
                return fmt::format_to(ctx.out(), "NOT_A_DIRECTORY");
            case Ichor::EtcdErrorCodes::KEY_ALREADY_EXISTS:
                return fmt::format_to(ctx.out(), "KEY_ALREADY_EXISTS");
            case Ichor::EtcdErrorCodes::ROOT_IS_READ_ONLY:
                return fmt::format_to(ctx.out(), "ROOT_IS_READ_ONLY");
            case Ichor::EtcdErrorCodes::DIRECTORY_NOT_EMPTY:
                return fmt::format_to(ctx.out(), "DIRECTORY_NOT_EMPTY");
            case Ichor::EtcdErrorCodes::PREVIOUS_VALUE_REQUIRED:
                return fmt::format_to(ctx.out(), "PREVIOUS_VALUE_REQUIRED");
            case Ichor::EtcdErrorCodes::TTL_IS_NOT_A_NUMBER:
                return fmt::format_to(ctx.out(), "TTL_IS_NOT_A_NUMBER");
            case Ichor::EtcdErrorCodes::INDEX_IS_NOT_A_NUMBER:
                return fmt::format_to(ctx.out(), "INDEX_IS_NOT_A_NUMBER");
            case Ichor::EtcdErrorCodes::INVALID_FIELD:
                return fmt::format_to(ctx.out(), "INVALID_FIELD");
            case Ichor::EtcdErrorCodes::INVALID_FORM:
                return fmt::format_to(ctx.out(), "INVALID_FORM");
            case Ichor::EtcdErrorCodes::RAFT_INTERNAL_ERROR:
                return fmt::format_to(ctx.out(), "RAFT_INTERNAL_ERROR");
            case Ichor::EtcdErrorCodes::LEADER_ELECTRION_ERROR:
                return fmt::format_to(ctx.out(), "LEADER_ELECTRION_ERROR");
            case Ichor::EtcdErrorCodes::WATCHER_CLEARED_DUE_TO_RECOVERY:
                return fmt::format_to(ctx.out(), "WATCHER_CLEARED_DUE_TO_RECOVERY");
            case Ichor::EtcdErrorCodes::EVENT_INDEX_OUTDATED_AND_CLEARED:
                return fmt::format_to(ctx.out(), "EVENT_INDEX_OUTDATED_AND_CLEARED");
            default:
                return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
        }
    }
};
