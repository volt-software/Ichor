#pragma once

#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/events/Event.h>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/executor.hpp>
#include <memory>

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace net = boost::asio;            // from <boost/asio/executor.hpp>

namespace Ichor::v1 {
    struct NewWsConnectionEvent final : public Event {
        explicit NewWsConnectionEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, std::shared_ptr<websocket::stream<beast::tcp_stream>> &&socket) noexcept :
                Event(_id, _originatingService, _priority), _socket(std::move(socket)) {}
        ~NewWsConnectionEvent() final = default;

        [[nodiscard]] std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] NameHashType get_type() const noexcept final {
            return TYPE;
        }

        static constexpr NameHashType TYPE = typeNameHash<NewWsConnectionEvent>();
        static constexpr std::string_view NAME = typeName<NewWsConnectionEvent>();

        std::shared_ptr<websocket::stream<beast::tcp_stream>> _socket;
    };
}

#endif
