#pragma once

#include <ichor/coroutines/Task.h>
#include <ichor/stl/StringUtils.h>
#include <string_view>
#include <tl/optional.h>
#include <tl/expected.h>

namespace Ichor {
    struct RedisAuthReply {
        bool success{};
    };

    struct RedisSetOptions {
        tl::optional<uint32_t> EX;
        tl::optional<uint32_t> PX;
        tl::optional<uint32_t> EXAT;
        tl::optional<uint32_t> PXAT;
        bool NX{};
        bool XX{};
        bool KEEPTTL{};
        bool GET{};
    };

    struct RedisSetReply {
        // if the set was succesful or not
        bool executed{};
        std::string oldValue;
    };

    struct RedisGetReply {
        // null if key not found
        tl::optional<std::string> value;
    };

    struct RedisIntegerReply {
        int64_t value;
    };

    enum class RedisError : uint_fast16_t {
        UNKNOWN,
        DISCONNECTED,
        FUNCTION_NOT_AVAILABLE_IN_SERVER, // probably redis-server version too low
        TRANSACTION_NOT_STARTED, // probably forgot to execute a multi command
        QUEUED, // Queued for transaction, use exec() to get the result
    };

    class IRedis {
    public:
        /// Authenticate as user
        /// \param key
        /// \param value
        /// \return
        virtual Task<tl::expected<RedisAuthReply, RedisError>> auth(std::string_view user, std::string_view password) = 0;

        /// Set key, value in Redis
        /// \param key
        /// \param value
        /// \return coroutine with the reply from Redis
        virtual Task<tl::expected<RedisSetReply, RedisError>> set(std::string_view key, std::string_view value) = 0;

        /// Set key, value in Redis
        /// \param key
        /// \param value
        /// \param opts
        /// \return coroutine with the reply from Redis
        virtual Task<tl::expected<RedisSetReply, RedisError>> set(std::string_view key, std::string_view value, RedisSetOptions const &opts) = 0;

        /// Get key
        /// \param key
        /// \return coroutine with the reply from Redis
        virtual Task<tl::expected<RedisGetReply, RedisError>> get(std::string_view key) = 0;

        /// Get key and delete the key
        /// \param key
        /// \return coroutine with the reply from Redis
        virtual Task<tl::expected<RedisGetReply, RedisError>> getdel(std::string_view key) = 0;

        /// Deletes a key in redis
        /// \param keys space-seperated list of keys to delete
        /// \return coroutine with the number of deleted values
        virtual Task<tl::expected<RedisIntegerReply, RedisError>> del(std::string_view keys) = 0;

        /// Increments a key in redis. If the key does not exist, it is set to 0 before performing the operation.
        /// \param key key to increment
        /// \return coroutine with the value of the key after increment
        virtual Task<tl::expected<RedisIntegerReply, RedisError>> incr(std::string_view key) = 0;

        /// Increments a key in redis by incr. If the key does not exist, it is set to 0 before performing the operation.
        /// \param key key to increment
        /// \param incr amount to increment with
        /// \return coroutine with the value of the key after increment
        virtual Task<tl::expected<RedisIntegerReply, RedisError>> incrBy(std::string_view key, int64_t incr) = 0;

        /// Increments a key in redis by incr. By using a negative increment value, the result is that the value stored at the key is decremented. If the key does not exist, it is set to 0 before performing the operation.
        /// \param key key to increment
        /// \param incr amount to increment with
        /// \return coroutine with the value of the key after increment
        virtual Task<tl::expected<RedisIntegerReply, RedisError>> incrByFloat(std::string_view key, double incr) = 0;

        /// Decrements a key in redis.  If the key does not exist, it is set to 0 before performing the operation.
        /// \param key key to decrement
        /// \return coroutine with the value of the key after decrement
        virtual Task<tl::expected<RedisIntegerReply, RedisError>> decr(std::string_view key) = 0;

        /// Decrements a key in redis by decr.  If the key does not exist, it is set to 0 before performing the operation.
        /// \param key key to decrement
        /// \param decr amount to decrement with
        /// \return coroutine with the value of the key after decrement
        virtual Task<tl::expected<RedisIntegerReply, RedisError>> decrBy(std::string_view key, int64_t decr) = 0;

        /// Returns the length of the string
        /// \param key
        /// \return coroutine with the length of the value stored for the key
        virtual Task<tl::expected<RedisIntegerReply, RedisError>> strlen(std::string_view key) = 0;

        /// Start a transaction
        /// \return coroutine with a possible error
        virtual Task<tl::expected<void, RedisError>> multi() = 0;

        /// Execute all commands in a transaction
        /// \return coroutine with a possible error
        virtual Task<tl::expected<std::vector<std::variant<RedisGetReply, RedisSetReply, RedisAuthReply, RedisIntegerReply>>, RedisError>> exec() = 0;

        /// Abort a transaction
        /// \return coroutine with a possible error
        virtual Task<tl::expected<void, RedisError>> discard() = 0;

        /// Returns information and statistics about the server
        /// \return coroutine with the length of the value stored for the key
        virtual Task<tl::expected<std::unordered_map<std::string, std::string>, RedisError>> info() = 0;

        virtual Task<tl::expected<Version, RedisError>> getServerVersion() = 0;

// getrange
// setrange
// append (multi?)
// append (multi?)
// bgsave
// bgrewriteaof
// lastsave
// client info
// dbsize
// acl setuser
// acl deluser
// acl users
// acl whoami
// acl save
// acl load
// acl log
// acl list
// acl getuser
// acl genpass
// config get
// config resetstat
// config rewrite
// config set
// exists
// expire
// expireat
// keys
// lpush
// lindex
// lset
// lrem
// lrange
// lpop
// lpos
// llen
// linsert
// mget
// mset
// move
// persist
// pexpire
// rename
// rpop
// rpush
// sort
// sort_ro
// select (db)
// sadd
// scard
// sdiff
// sinter
// sismember
// smismember
// smembers
// smove
// spop
// srem
// sunioin
// touch
// ttl
// type
// unlink
// wait?
// waitaof
    protected:
        ~IRedis() = default;
    };
}
