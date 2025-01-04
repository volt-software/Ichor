#pragma once

#ifndef ICHOR_USE_BOOST_BEAST
#error "Boost not enabled."
#endif

#if BOOST_VERSION >= 108000
#define ASIO_SPAWN_COMPLETION_TOKEN , [](std::exception_ptr e) { if (e) std::rethrow_exception(e); }
#else
#define ASIO_SPAWN_COMPLETION_TOKEN
#endif

#include <ichor/event_queues/IEventQueue.h>
#include <boost/asio/spawn.hpp>

namespace net = boost::asio;            // from <boost/asio.hpp>

namespace Ichor {
    class IBoostAsioQueue : public IEventQueue {
    public:
        ~IBoostAsioQueue() override = default;
        virtual net::io_context& getContext() noexcept = 0;
        [[nodiscard]] virtual bool fibersShouldStop() const noexcept = 0;
    };
}
