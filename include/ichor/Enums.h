#pragma once

#include <fmt/core.h>

namespace Ichor
{
    namespace Detail {
        enum class DependencyChange {
            NOT_FOUND,
            FOUND,
            FOUND_AND_START_ME,
            FOUND_AND_STOP_ME,
        };
    }

    enum class ServiceState {
        UNINSTALLED,
        INSTALLED,
        STARTING,
        STOPPING,
        INJECTING,
        UNINJECTING,
        ACTIVE,
    };

    enum class StartBehaviour {
        FAILED_DO_NOT_RETRY,
        FAILED_AND_RETRY,
        SUCCEEDED
    };

    enum class LogLevel {
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERROR
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
    enum class state
    {
        value_not_ready_consumer_active,
        value_not_ready_consumer_suspended,
        value_ready_producer_active,
        value_ready_producer_suspended,
        cancelled
    };
}

template <>
struct fmt::formatter<Ichor::ServiceState>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::ServiceState& state, FormatContext& ctx)
    {
        switch(state)
        {
            case Ichor::ServiceState::UNINSTALLED:
                return format_to(ctx.out(), "UNINSTALLED");
            case Ichor::ServiceState::INSTALLED:
                return format_to(ctx.out(), "INSTALLED");
            case Ichor::ServiceState::STARTING:
                return format_to(ctx.out(), "STARTING");
            case Ichor::ServiceState::STOPPING:
                return format_to(ctx.out(), "STOPPING");
            case Ichor::ServiceState::INJECTING:
                return format_to(ctx.out(), "INJECTING");
            case Ichor::ServiceState::UNINJECTING:
                return format_to(ctx.out(), "UNINJECTING");
            case Ichor::ServiceState::ACTIVE:
                return format_to(ctx.out(), "ACTIVE");
            default:
                return format_to(ctx.out(), "error, please file a bug in Ichor");
        }
    }
};

template <>
struct fmt::formatter<Ichor::StartBehaviour>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::StartBehaviour& behaviour, FormatContext& ctx)
    {
        switch(behaviour)
        {
            case Ichor::StartBehaviour::FAILED_DO_NOT_RETRY:
                return format_to(ctx.out(), "FAILED_DO_NOT_RETRY");
            case Ichor::StartBehaviour::FAILED_AND_RETRY:
                return format_to(ctx.out(), "FAILED_AND_RETRY");
            case Ichor::StartBehaviour::SUCCEEDED:
                return format_to(ctx.out(), "ACTIVE");
            default:
                return format_to(ctx.out(), "error, please file a bug in Ichor");
        }
    }
};

template <>
struct fmt::formatter<Ichor::state>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::state& state, FormatContext& ctx)
    {
        switch(state)
        {
            case Ichor::state::value_not_ready_consumer_active:
                return format_to(ctx.out(), "value_not_ready_consumer_active");
            case Ichor::state::value_not_ready_consumer_suspended:
                return format_to(ctx.out(), "value_not_ready_consumer_suspended");
            case Ichor::state::value_ready_producer_active:
                return format_to(ctx.out(), "value_ready_producer_active");
            case Ichor::state::value_ready_producer_suspended:
                return format_to(ctx.out(), "value_ready_producer_suspended");
            case Ichor::state::cancelled:
                return format_to(ctx.out(), "cancelled");
            default:
                return format_to(ctx.out(), "error, please file a bug in Ichor");
        }
    }
};

template <>
struct fmt::formatter<Ichor::Detail::DependencyChange>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::Detail::DependencyChange& change, FormatContext& ctx)
    {
        switch(change)
        {
            case Ichor::Detail::DependencyChange::NOT_FOUND:
                return format_to(ctx.out(), "NOT_FOUND");
            case Ichor::Detail::DependencyChange::FOUND:
                return format_to(ctx.out(), "FOUND");
            case Ichor::Detail::DependencyChange::FOUND_AND_START_ME:
                return format_to(ctx.out(), "FOUND_AND_START_ME");
            case Ichor::Detail::DependencyChange::FOUND_AND_STOP_ME:
                return format_to(ctx.out(), "FOUND_AND_STOP_ME");
            default:
                return format_to(ctx.out(), "error, please file a bug in Ichor");
        }
    }
};
