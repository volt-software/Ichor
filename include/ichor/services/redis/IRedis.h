#pragma once

#include <ichor/coroutines/AsyncGenerator.h>
#include <string_view>
#include <optional>

namespace Ichor {
    struct RedisAuthReply {
        bool success{};
    };

    struct RedisSetOptions {
        std::optional<uint32_t> EX;
        std::optional<uint32_t> PX;
        std::optional<uint32_t> EXAT;
        std::optional<uint32_t> PXAT;
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
        std::optional<std::string> value;
    };

    struct RedisIntegerReply {
        int64_t value;
    };

    class IRedis {
    public:
        virtual void setPriority(uint64_t priority) = 0;
        virtual uint64_t getPriority() = 0;

        /// Authenticate as user
        /// \param key
        /// \param value
        /// \return
        virtual AsyncGenerator<RedisAuthReply> auth(std::string_view user, std::string_view password) = 0;

        /// Set key, value in Redis
        /// \param key
        /// \param value
        /// \return coroutine with the reply from Redis
        virtual AsyncGenerator<RedisSetReply> set(std::string_view key, std::string_view value) = 0;

        /// Set key, value in Redis
        /// \param key
        /// \param value
        /// \param opts
        /// \return coroutine with the reply from Redis
        virtual AsyncGenerator<RedisSetReply> set(std::string_view key, std::string_view value, RedisSetOptions const &opts) = 0;

        /// Set key, value in Redis without waiting for reply
        /// \param key
        /// \param value
        virtual void setAndForget(std::string_view key, std::string_view value) = 0;

        /// Set key, value in Redis without waiting for reply
        /// \param key
        /// \param value
        /// \param opts
        virtual void setAndForget(std::string_view key, std::string_view value, RedisSetOptions const &opts) = 0;

        virtual AsyncGenerator<RedisGetReply> get(std::string_view key) = 0;

        /// Deletes a key in redis
        /// \param keys space-seperated list of keys to delete
        /// \return coroutine with the number of deleted values
        virtual AsyncGenerator<RedisIntegerReply> del(std::string_view keys) = 0;

        /// Increments a key in redis. If the key does not exist, it is set to 0 before performing the operation.
        /// \param key key to increment
        /// \return coroutine with the value of the key after increment
        virtual AsyncGenerator<RedisIntegerReply> incr(std::string_view key) = 0;

        /// Increments a key in redis by incr. If the key does not exist, it is set to 0 before performing the operation.
        /// \param key key to increment
        /// \param incr amount to increment with
        /// \return coroutine with the value of the key after increment
        virtual AsyncGenerator<RedisIntegerReply> incrBy(std::string_view key, int64_t incr) = 0;

        /// Increments a key in redis by incr. By using a negative increment value, the result is that the value stored at the key is decremented. If the key does not exist, it is set to 0 before performing the operation.
        /// \param key key to increment
        /// \param incr amount to increment with
        /// \return coroutine with the value of the key after increment
        virtual AsyncGenerator<RedisIntegerReply> incrByFloat(std::string_view key, double incr) = 0;

        /// Decrements a key in redis.  If the key does not exist, it is set to 0 before performing the operation.
        /// \param key key to decrement
        /// \return coroutine with the value of the key after decrement
        virtual AsyncGenerator<RedisIntegerReply> decr(std::string_view key) = 0;

        /// Decrements a key in redis by decr.  If the key does not exist, it is set to 0 before performing the operation.
        /// \param key key to decrement
        /// \param decr amount to decrement with
        /// \return coroutine with the value of the key after decrement
        virtual AsyncGenerator<RedisIntegerReply> decrBy(std::string_view key, int64_t decr) = 0;

    protected:
        ~IRedis() = default;
    };
}
