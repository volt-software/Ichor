#pragma once

#ifdef USE_BOOST_BEAST

#include <ichor/Events.h>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/executor.hpp>
#include <ichor/optional_bundles/network_bundle/ws/WsCopyIsMoveWorkaround.h>

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace net = boost::asio;            // from <boost/asio/executor.hpp>

namespace Ichor {
    struct NewWsConnectionEvent final : public Event {
        explicit NewWsConnectionEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, CopyIsMoveWorkaround<tcp::socket> &&socket, net::executor executor) noexcept :
                Event(TYPE, NAME, _id, _originatingService, _priority), _socket(std::forward<CopyIsMoveWorkaround<tcp::socket>>(socket)), _executor(std::move(executor)) {}
        ~NewWsConnectionEvent() final = default;

        static constexpr uint64_t TYPE = typeNameHash<NewWsConnectionEvent>();
        static constexpr std::string_view NAME = typeName<NewWsConnectionEvent>();

        mutable CopyIsMoveWorkaround<tcp::socket> _socket;
        net::executor _executor;
    };
}

#endif