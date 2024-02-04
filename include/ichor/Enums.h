#pragma once

#include <fmt/core.h>
#include <cstdint>

namespace Ichor
{
    namespace Detail {
        enum class DependencyChange : uint_fast16_t {
            NOT_FOUND,
            FOUND,
            FOUND_AND_START_ME,
            FOUND_AND_STOP_ME,
        };
    }

    enum class ServiceState : uint_fast16_t {
        UNINSTALLED,
        INSTALLED,
        INJECTING,
        STARTING,
        ACTIVE,
        UNINJECTING,
        STOPPING,
    };

    enum class LogLevel : uint_fast16_t {
        LOG_TRACE,
        LOG_DEBUG,
        LOG_INFO,
        LOG_WARN,
        LOG_ERROR
    };

    // State transition diagram
    //   VNRCA - value_not_ready_consumer_active
    //   VNRCS - value_not_ready_consumer_suspended
    //   VRPA  - value_ready_producer_active
    //   VRPS  - value_ready_producer_suspended
    //
    //       A         +---  VNRCA --[C]--> VNRCS   yield_value()
    //       |         |     |  A           |  A       |   .
    //       |        [C]   [P] |          [P] |       |   .
    //       |         |     | [C]          | [C]      |   .
    //       |         |     V  |           V  |       |   .
    //  operator++/    |     VRPS <--[P]--- VRPA       V   |
    //  begin()        |      |              |             |
    //                 |     [C]            [C]            |
    //                 |      +----+     +---+             |
    //                 |           |     |                 |
    //                 |           V     V                 V
    //                 +--------> cancelled         ~async_generator()
    //
    // [C] - Consumer performs this transition
    // [P] - Producer performs this transition
    enum class state : uint_fast16_t {
        unknown,
        value_not_ready_consumer_active,
        value_not_ready_consumer_suspended,
        value_ready_producer_active,
        value_ready_producer_suspended,
        cancelled
    };

    // coroutine specific 'enums'
    struct Empty{};
    constexpr static Empty empty = {};

    enum class StartBehaviour : uint_fast16_t {
        DONE,
        STARTED,
        STOPPED
    };

    enum class WaitError : uint_fast16_t {
        QUITTING
    };

    enum class StartError : uint_fast16_t {
        FAILED
    };

    // Necessary to prevent excessive events on the queue.
    // Every async call using this will end up adding an event to the queue on co_return/co_yield.
    enum class IchorBehaviour : uint_fast16_t {
        DONE
    };

}

template <>
struct fmt::formatter<Ichor::ServiceState> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::ServiceState& state, FormatContext& ctx) {
        switch(state) {
            case Ichor::ServiceState::UNINSTALLED:
                return fmt::format_to(ctx.out(), "UNINSTALLED");
            case Ichor::ServiceState::INSTALLED:
                return fmt::format_to(ctx.out(), "INSTALLED");
            case Ichor::ServiceState::STARTING:
                return fmt::format_to(ctx.out(), "STARTING");
            case Ichor::ServiceState::STOPPING:
                return fmt::format_to(ctx.out(), "STOPPING");
            case Ichor::ServiceState::INJECTING:
                return fmt::format_to(ctx.out(), "INJECTING");
            case Ichor::ServiceState::UNINJECTING:
                return fmt::format_to(ctx.out(), "UNINJECTING");
            case Ichor::ServiceState::ACTIVE:
                return fmt::format_to(ctx.out(), "ACTIVE");
            default:
                return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
        }
    }
};

template <>
struct fmt::formatter<Ichor::state> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::state& state, FormatContext& ctx) {
        switch(state) {
            case Ichor::state::unknown:
                return fmt::format_to(ctx.out(), "unknown");
            case Ichor::state::value_not_ready_consumer_active:
                return fmt::format_to(ctx.out(), "value_not_ready_consumer_active");
            case Ichor::state::value_not_ready_consumer_suspended:
                return fmt::format_to(ctx.out(), "value_not_ready_consumer_suspended");
            case Ichor::state::value_ready_producer_active:
                return fmt::format_to(ctx.out(), "value_ready_producer_active");
            case Ichor::state::value_ready_producer_suspended:
                return fmt::format_to(ctx.out(), "value_ready_producer_suspended");
            case Ichor::state::cancelled:
                return fmt::format_to(ctx.out(), "cancelled");
            default:
                return fmt::format_to(ctx.out(), "error, please file a bug in Ichor, val: {}", (int)state);
        }
    }
};

template <>
struct fmt::formatter<Ichor::Detail::DependencyChange> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::Detail::DependencyChange& change, FormatContext& ctx) {
        switch(change) {
            case Ichor::Detail::DependencyChange::NOT_FOUND:
                return fmt::format_to(ctx.out(), "NOT_FOUND");
            case Ichor::Detail::DependencyChange::FOUND:
                return fmt::format_to(ctx.out(), "FOUND");
            case Ichor::Detail::DependencyChange::FOUND_AND_START_ME:
                return fmt::format_to(ctx.out(), "FOUND_AND_START_ME");
            case Ichor::Detail::DependencyChange::FOUND_AND_STOP_ME:
                return fmt::format_to(ctx.out(), "FOUND_AND_STOP_ME");
            default:
                return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
        }
    }
};

template <>
struct fmt::formatter<Ichor::StartBehaviour> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::StartBehaviour& change, FormatContext& ctx) {
        switch(change) {
            case Ichor::StartBehaviour::DONE:
                return fmt::format_to(ctx.out(), "DONE");
            case Ichor::StartBehaviour::STARTED:
                return fmt::format_to(ctx.out(), "STARTED");
            case Ichor::StartBehaviour::STOPPED:
                return fmt::format_to(ctx.out(), "STOPPED");
            default:
                return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
        }
    }
};

template <>
struct fmt::formatter<Ichor::LogLevel> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::LogLevel& change, FormatContext& ctx) {
        switch(change) {
            case Ichor::LogLevel::LOG_TRACE:
                return fmt::format_to(ctx.out(), "LOG_TRACE");
            case Ichor::LogLevel::LOG_DEBUG:
                return fmt::format_to(ctx.out(), "LOG_DEBUG");
            case Ichor::LogLevel::LOG_INFO:
                return fmt::format_to(ctx.out(), "LOG_INFO");
            case Ichor::LogLevel::LOG_WARN:
                return fmt::format_to(ctx.out(), "LOG_WARN");
            case Ichor::LogLevel::LOG_ERROR:
                return fmt::format_to(ctx.out(), "LOG_ERROR");
            default:
                return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
        }
    }
};
