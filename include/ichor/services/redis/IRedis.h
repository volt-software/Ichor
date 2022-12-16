#pragma once

#include <ichor/Service.h>

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

    protected:
        ~IRedis() = default;
    };
}
