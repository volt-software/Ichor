#pragma once

#ifndef ICHOR_USE_BOOST_BEAST
#error "Boost not enabled."
#endif

#include <ichor/event_queues/IBoostAsioQueue.h>
#include <memory>
#include <thread>

namespace Ichor {
    class BoostAsioQueue final : public IBoostAsioQueue {
    public:
        BoostAsioQueue();
        explicit BoostAsioQueue(uint64_t unused);
        ~BoostAsioQueue() final;

        void pushEventInternal(uint64_t priority, std::unique_ptr<Event> &&event) final;

        [[nodiscard]] bool empty() const final;
        [[nodiscard]] uint64_t size() const final;
        [[nodiscard]] bool is_running() const noexcept final;
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR NameHashType get_queue_name_hash() const noexcept final;

        bool start(bool captureSigInt) final;
        [[nodiscard]] bool shouldQuit() final;
        void quit() final;

        net::io_context& getContext() noexcept final;
        [[nodiscard]] bool fibersShouldStop() const noexcept final;

    private:
        void shouldAddQuitEvent();

        net::io_context _context;
        std::atomic<bool> _quit{};
        std::thread::id _threadId{};
        std::chrono::steady_clock::time_point _whenQuitEventWasSent{};
        std::atomic<bool> _quitEventSent{false};
        uint64_t _quitTimeoutMs{};
    };
}
