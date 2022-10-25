#pragma once

#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/events/Event.h>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/executor.hpp>
#include <ichor/services/network/ws/WsCopyIsMoveWorkaround.h>

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace net = boost::asio;            // from <boost/asio/executor.hpp>

namespace Ichor {
    struct NewWsConnectionEvent final : public Event {
        explicit NewWsConnectionEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, CopyIsMoveWorkaround<tcp::socket> &&socket) noexcept :
                Event(TYPE, NAME, _id, _originatingService, _priority), _socket(std::forward<CopyIsMoveWorkaround<tcp::socket>>(socket)) {}
        ~NewWsConnectionEvent() final = default;

        static constexpr uint64_t TYPE = typeNameHash<NewWsConnectionEvent>();
        static constexpr std::string_view NAME = typeName<NewWsConnectionEvent>();

        mutable CopyIsMoveWorkaround<tcp::socket> _socket;
    };
}

#endif